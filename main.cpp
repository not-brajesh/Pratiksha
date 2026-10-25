#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <csignal>
#include <algorithm>
#include <cctype>
#include <climits>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// ============================================================
// PRATIKSHA — Patient Routing & Intelligent Test Queue Handling
// Backend v1.3
//
// Changes over v1.2:
//   - Event-based simulation (no per-minute tick)
//   - Global scheduler (scheduleStep) instead of per-centre greedy
//   - Configurable MOVE_TIME_MINUTES between tests
//   - Patient.currentCentre/currentTestId for real-state tracking
//   - handleNextPatient atomically reserves assignment
//   - handleCompleteTest validates centre assignment
//   - /api/queue returns metrics (makespan, Σcompletion, Σwaiting)
// ============================================================

namespace Config {
    constexpr int PORT = 8080;
    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 32768;
    constexpr int QUEUE_MAX_TIME = 5000;   // safety cap
    constexpr long INF_TIME = 1000000000L;
    constexpr double LAMBDA = 0.5;         // load-balance weight
    constexpr int MOVE_TIME_MINUTES = 0;   // between tests; set >0 for hospital layout

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.3.0";
}

// ------------------------------------------------------------
// Models
// ------------------------------------------------------------

enum class TestStatus { WAITING, COMPLETED };
enum class PatientStatus { WAITING, COMPLETED };

struct DiagnosticTest {
    string id;
    string name;
    int durationMinutes;
};

struct PatientTestState {
    string testId;
    TestStatus status = TestStatus::WAITING;
    long completedAt = -1;
};

struct Patient {
    string id;
    string name;
    vector<PatientTestState> testStates;
    long addedSeq = 0;
    long arrivalTime = 0;
    PatientStatus status = PatientStatus::WAITING;
    bool emergency = false;

    // Real-state tracking (which centre/test is this patient in right now)
    string currentCentre = "";
    string currentTestId = "";
};

struct TestingCentre {
    string id;
    string name;
    string testId;
    int durationMinutes = 0;
    string currentPatient = "";
    string currentTestId = "";
    int remainingMin = 0;
    long busyMinutes = 0;
    int completedCount = 0;

    bool available() const { return currentPatient.empty(); }
};

struct QueueStep {
    string testId;
    int startTime;
    int duration;
};

struct QueueEvent {
    int time;
    string type;
    string centreId;
    string patientId;
};

struct CentreState {
    string id;
    string name;
    int duration;
    long busyMinutes;
    int completedCount;
};

struct QueueMetrics {
    int makespan = 0;
    long sumCompletionTimes = 0;
    long sumWaitingTimes = 0;
    int patientsCompleted = 0;
};

struct QueueResult {
    int totalTime = 0;
    vector<CentreState> centres;
    map<string, vector<QueueStep>> perPatient;
    vector<QueueEvent> events;
    QueueMetrics metrics;
};

// ------------------------------------------------------------
// Server State
// ------------------------------------------------------------

class ServerState {
public:
    vector<DiagnosticTest> tests;
    vector<TestingCentre> centres;
    vector<Patient> patients;
    long nextSeq = 0;

    ServerState() {
        tests = {
            {"A", "X-Ray", 10},
            {"B", "Blood Test", 5},
            {"C", "Ultrasound", 20},
            {"D", "CT Scan", 30}
        };
        centres = {
            {"A1", "X-Ray Room 1", "A", 10},
            {"A2", "X-Ray Room 2", "A", 10},
            {"B1", "Blood Test Lab", "B", 5},
            {"C1", "Ultrasound Room", "C", 20},
            {"D1", "CT Scan Room", "D", 30}
        };
    }

    bool testExists(const string& id) const {
        for (const auto& t : tests) if (t.id == id) return true;
        return false;
    }
    int testDuration(const string& id) const {
        for (const auto& t : tests) if (t.id == id) return t.durationMinutes;
        return 0;
    }
    string testName(const string& id) const {
        for (const auto& t : tests) if (t.id == id) return t.name;
        return id;
    }
    TestingCentre* findCentre(const string& id) {
        for (auto& c : centres) if (c.id == id) return &c;
        return nullptr;
    }
    const TestingCentre* findCentre(const string& id) const {
        for (const auto& c : centres) if (c.id == id) return &c;
        return nullptr;
    }
    bool hasPatient(const string& id) const {
        for (const auto& p : patients) if (p.id == id) return true;
        return false;
    }
    Patient* findPatient(const string& id) {
        for (auto& p : patients) if (p.id == id) return &p;
        return nullptr;
    }

    bool addPatient(
        const string& id, const string& name,
        const vector<string>& rawTests, bool emergency
    ) {
        if (id.empty() || hasPatient(id)) return false;
        vector<string> unique;
        set<string> seen;
        for (const auto& t : rawTests) {
            if (!testExists(t)) return false;
            if (seen.count(t)) continue;
            seen.insert(t);
            unique.push_back(t);
        }
        if (unique.empty()) return false;

        Patient p;
        p.id = id;
        p.name = name;
        p.addedSeq = nextSeq++;
        p.arrivalTime = p.addedSeq;
        p.status = PatientStatus::WAITING;
        p.emergency = emergency;
        for (const auto& t : unique) {
            PatientTestState ts;
            ts.testId = t;
            ts.status = TestStatus::WAITING;
            p.testStates.push_back(ts);
        }
        patients.push_back(p);
        return true;
    }

    bool removePatient(const string& id) {
        for (size_t i = 0; i < patients.size(); ++i) {
            if (patients[i].id == id) {
                for (auto& c : centres) {
                    if (c.currentPatient == id) {
                        c.currentPatient = "";
                        c.currentTestId = "";
                        c.remainingMin = 0;
                    }
                }
                patients.erase(patients.begin() + i);
                for (size_t j = 0; j < patients.size(); ++j) {
                    patients[j].addedSeq = (long)j;
                    patients[j].arrivalTime = (long)j;
                }
                return true;
            }
        }
        return false;
    }

    void reset() {
        patients.clear();
        nextSeq = 0;
        for (auto& c : centres) {
            c.currentPatient = "";
            c.currentTestId = "";
            c.remainingMin = 0;
            c.busyMinutes = 0;
            c.completedCount = 0;
        }
    }
};

// ------------------------------------------------------------
// JSON Helpers
// ------------------------------------------------------------

string escapeJson(const string& s) {
    ostringstream out;
    for (char c : s) {
        switch (c) {
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n";  break;
            case '\r': out << "\\r";  break;
            case '\t': out << "\\t";  break;
            default:
                if ((unsigned char)c >= 0x20) out << c;
        }
    }
    return out.str();
}

string jsonGetString(const string& json, const string& key) {
    string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == string::npos) return "";
    pos = json.find(':', pos + needle.size());
    if (pos == string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == string::npos) return "";
    size_t end = json.find('"', pos + 1);
    if (end == string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

vector<string> jsonGetStringArray(const string& json, const string& key) {
    vector<string> out;
    string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == string::npos) return out;
    pos = json.find('[', pos);
    if (pos == string::npos) return out;
    size_t end = json.find(']', pos);
    if (end == string::npos) return out;
    string arr = json.substr(pos + 1, end - pos - 1);
    size_t p = 0;
    while ((p = arr.find('"', p)) != string::npos) {
        size_t e = arr.find('"', p + 1);
        if (e == string::npos) break;
        out.push_back(arr.substr(p + 1, e - p - 1));
        p = e + 1;
    }
    return out;
}

bool jsonGetBool(const string& json, const string& key) {
    string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == string::npos) return false;
    pos = json.find(':', pos + needle.size());
    if (pos == string::npos) return false;
    pos++;
    while (pos < json.size() && isspace((unsigned char)json[pos])) pos++;
    return json.compare(pos, 4, "true") == 0;
}

// ------------------------------------------------------------
// HTTP helpers
// ------------------------------------------------------------

string makeResponse(int code, const string& text,
                    const string& ct, const string& body) {
    ostringstream r;
    r << "HTTP/1.1 " << code << " " << text << "\r\n"
      << "Content-Type: " << ct << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "Cache-Control: no-store\r\n"
      << "X-Content-Type-Options: nosniff\r\n\r\n" << body;
    return r.str();
}
string jsonResponse(int c, const string& t, const string& j) {
    return makeResponse(c, t, "application/json; charset=utf-8", j);
}
string errorResponse(int c, const string& t, const string& m) {
    return jsonResponse(c, t, "{\"success\":false,\"error\":\"" + escapeJson(m) + "\"}");
}

bool sendAll(int sock, const string& r) {
    size_t sent = 0;
    while (sent < r.size()) {
        ssize_t n = ::send(sock, r.data() + sent, r.size() - sent, 0);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        if (n == 0) return false;
        sent += (size_t)n;
    }
    return true;
}

long parseContentLength(const string& headers) {
    size_t pos = 0;
    while ((pos = headers.find("Content-Length:", pos)) != string::npos) {
        bool lineStart = (pos == 0) || (headers[pos - 1] == '\n');
        if (lineStart) {
            size_t vs = pos + 15;
            size_t ve = headers.find("\r\n", vs);
            if (ve == string::npos) ve = headers.size();
            string v = headers.substr(vs, ve - vs);
            while (!v.empty() && isspace((unsigned char)v.front())) v.erase(0, 1);
            while (!v.empty() && isspace((unsigned char)v.back()))  v.pop_back();
            try { return stol(v); } catch (...) { return -1; }
        }
        pos++;
    }
    return -1;
}

string readRequest(int sock) {
    string req;
    char buf[4096];
    size_t he = string::npos;
    long cl = -1;
    while (req.size() < Config::MAX_REQUEST_SIZE) {
        if (he != string::npos) {
            size_t bs = he + 4;
            size_t br = req.size() > bs ? req.size() - bs : 0;
            if (cl <= 0 || br >= (size_t)cl) break;
        }
        ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
        if (n < 0) { if (errno == EINTR) continue; return ""; }
        if (n == 0) break;
        req.append(buf, (size_t)n);
        if (he == string::npos) {
            he = req.find("\r\n\r\n");
            if (he != string::npos) cl = parseContentLength(req.substr(0, he));
        }
    }
    return req;
}

struct HttpRequest {
    string method, path, version, headers, body;
};

bool parseRequestLine(const string& req, HttpRequest& p) {
    size_t le = req.find("\r\n");
    if (le == string::npos) return false;
    istringstream in(req.substr(0, le));
    if (!(in >> p.method >> p.path >> p.version)) return false;
    string extra;
    if (in >> extra) return false;
    if (p.version != "HTTP/1.1" && p.version != "HTTP/1.0") return false;
    size_t he = req.find("\r\n\r\n");
    if (he != string::npos) {
        p.headers = req.substr(le + 2, he - (le + 2));
        p.body = req.substr(he + 4);
    }
    return true;
}

bool matchPrefix(const string& path, const string& prefix, string& rest) {
    if (path.size() > prefix.size() && path.compare(0, prefix.size(), prefix) == 0) {
        rest = path.substr(prefix.size());
        return true;
    }
    return false;
}

// ------------------------------------------------------------
// Simulation types (used only inside computeQueue / handleNextPatient)
// ------------------------------------------------------------

struct SimPatient {
    string id;
    vector<string> remaining;
    long addedSeq = 0;
    bool emergency = false;

    int totalWaitingMinutes = 0;
    int testsCompleted = 0;
    bool inService = false;
    string currentCentreId = "";
    long readyAt = 0;   // earliest time (sim-minutes) this patient can start next test
};

struct SimCentre {
    string id;
    string name;
    string testId;
    int duration = 0;
    string currentPatient = "";
    long busyUntil = 0;      // absolute sim-time when this centre frees
    long busyMinutes = 0;
    int completedCount = 0;
};

struct SimAssignment {
    string patientId;
    string centreId;
    string testId;
    int duration = 0;
    double priority = 0.0;
};

// ------------------------------------------------------------
// Scheduling core
// ------------------------------------------------------------
//
// Two-tier priority (higher = pick first):
//     base   = (currentTime + (maxSeq - addedSeq)) * 100     [age / fairness]
//     emerg  = +1e12 if emergency
//     short  = +1000 / duration                              [prefer shorter]
//     load   = -LAMBDA * (waitingForTest / numCentresForTest) [spread contention]
//
// Global step: generates all eligible (patient, centre) pairs,
// sorts by priority desc, greedily picks a matching (no
// patient/centre reuse). Non-greedy exhaustive matching deferred.
// ------------------------------------------------------------

double computePriority(
    const SimPatient& p,
    const SimCentre& c,
    long currentTime,
    long maxSeq,
    int waitingForTest,
    int centresForTest
) {
    double wait = (double)(currentTime + (maxSeq - p.addedSeq));
    double score = wait * 100.0;
    if (p.emergency) score += 1e12;
    if (c.duration > 0) score += 1000.0 / c.duration;
    if (centresForTest > 0) {
        double load = (double)waitingForTest / (double)centresForTest;
        score -= Config::LAMBDA * load;
    }
    return score;
}

vector<SimAssignment> scheduleStep(
    vector<SimPatient>& patients,
    vector<SimCentre>& centres,
    long currentTime,
    long maxSeq
) {
    vector<SimAssignment> candidates;

    for (auto& c : centres) {
        if (!c.currentPatient.empty()) continue;
        if (c.busyUntil > currentTime) continue;

        int waitingForTest = 0;
        for (auto& p : patients) {
            if (p.inService) continue;
            if (p.readyAt > currentTime) continue;
            for (auto& t : p.remaining) {
                if (t == c.testId) { waitingForTest++; break; }
            }
        }
        if (waitingForTest == 0) continue;

        int centresForTest = 0;
        for (auto& other : centres) if (other.testId == c.testId) centresForTest++;

        for (auto& p : patients) {
            if (p.inService) continue;
            if (p.readyAt > currentTime) continue;
            bool needs = false;
            for (auto& t : p.remaining) {
                if (t == c.testId) { needs = true; break; }
            }
            if (!needs) continue;

            SimAssignment a;
            a.patientId = p.id;
            a.centreId = c.id;
            a.testId = c.testId;
            a.duration = c.duration;
            a.priority = computePriority(p, c, currentTime, maxSeq, waitingForTest, centresForTest);
            candidates.push_back(a);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const SimAssignment& a, const SimAssignment& b) {
            return a.priority > b.priority;
        });

    vector<SimAssignment> result;
    set<string> usedCentres, usedPatients;
    for (auto& a : candidates) {
        if (usedCentres.count(a.centreId)) continue;
        if (usedPatients.count(a.patientId)) continue;
        result.push_back(a);
        usedCentres.insert(a.centreId);
        usedPatients.insert(a.patientId);
    }
    return result;
}

// ------------------------------------------------------------
// computeQueue — event-based simulation (read-only)
// ------------------------------------------------------------

QueueResult computeQueue(const ServerState& state) {
    QueueResult result;

    long maxSeq = state.nextSeq > 0 ? state.nextSeq - 1 : 0;

    vector<SimPatient> sim;
    for (const auto& p : state.patients) {
        SimPatient sp;
        sp.id = p.id;
        sp.addedSeq = p.addedSeq;
        sp.emergency = p.emergency;
        for (const auto& ts : p.testStates) {
            if (ts.status == TestStatus::WAITING) sp.remaining.push_back(ts.testId);
        }
        if (!sp.remaining.empty()) {
            sim.push_back(sp);
            result.perPatient[p.id] = {};
        }
    }

    vector<SimCentre> simC;
    for (const auto& c : state.centres) {
        SimCentre sc;
        sc.id = c.id;
        sc.name = c.name;
        sc.testId = c.testId;
        sc.duration = c.durationMinutes;
        sc.busyMinutes = c.busyMinutes;
        sc.completedCount = c.completedCount;
        if (!c.currentPatient.empty()) {
            // Real state has an assignment in progress — sim starts with it "running"
            sc.currentPatient = c.currentPatient;
            sc.busyUntil = (c.remainingMin > 0 ? (long)c.remainingMin : (long)c.durationMinutes);
        }
        simC.push_back(sc);
    }

    long time = 0;
    while (time < Config::QUEUE_MAX_TIME) {
        // (1) Completions
        for (auto& c : simC) {
            if (!c.currentPatient.empty() && c.busyUntil <= time) {
                long completeTime = c.busyUntil;
                for (auto& p : sim) {
                    if (p.id == c.currentPatient) {
                        for (auto it = p.remaining.begin(); it != p.remaining.end(); ++it) {
                            if (*it == c.testId) { p.remaining.erase(it); break; }
                        }
                        p.inService = false;
                        p.testsCompleted++;
                        p.readyAt = completeTime + Config::MOVE_TIME_MINUTES;
                        p.currentCentreId = "";
                        break;
                    }
                }
                result.events.push_back({
                    (int)completeTime, "complete", c.id, c.currentPatient
                });
                c.completedCount++;
                c.busyMinutes += c.duration;
                c.currentPatient = "";
                c.busyUntil = 0;
            }
        }

        // (2) All done?
        bool allDone = true;
        for (auto& p : sim) if (!p.remaining.empty()) { allDone = false; break; }
        if (allDone) break;

        // (3) Global assignment step
        auto assigns = scheduleStep(sim, simC, time, maxSeq);
        for (auto& a : assigns) {
            SimCentre* c = nullptr;
            SimPatient* p = nullptr;
            for (auto& x : simC) if (x.id == a.centreId) { c = &x; break; }
            for (auto& x : sim) if (x.id == a.patientId) { p = &x; break; }
            if (!c || !p) continue;

            c->currentPatient = p->id;
            c->busyUntil = time + c->duration;
            p->inService = true;
            p->currentCentreId = c->id;

            result.events.push_back({(int)time, "start", c->id, p->id});
            result.perPatient[p->id].push_back({
                c->testId, (int)time, c->duration
            });
        }

        // (4) Advance to next event
        long nextTime = Config::INF_TIME;
        for (auto& c : simC)
            if (!c.currentPatient.empty() && c.busyUntil > time)
                nextTime = min(nextTime, c.busyUntil);
        for (auto& p : sim) {
            if (!p.inService && !p.remaining.empty() && p.readyAt > time)
                nextTime = min(nextTime, p.readyAt);
        }
        if (nextTime == Config::INF_TIME) break;
        if (nextTime <= time) nextTime = time + 1;
        time = nextTime;
    }

    result.totalTime = (int)time;

    // Final centre states
    for (const auto& sc : simC) {
        CentreState cs;
        cs.id = sc.id;
        cs.name = sc.name;
        cs.duration = sc.duration;
        cs.busyMinutes = sc.busyMinutes;
        cs.completedCount = sc.completedCount;
        result.centres.push_back(cs);
    }

    // Metrics
    for (auto& kv : result.perPatient) {
        long maxEnd = 0;
        long totalDur = 0;
        for (auto& step : kv.second) {
            long end = step.startTime + step.duration;
            if (end > maxEnd) maxEnd = end;
            totalDur += step.duration;
        }
        if (maxEnd > result.metrics.makespan) result.metrics.makespan = (int)maxEnd;
        result.metrics.sumCompletionTimes += maxEnd;
        long w = maxEnd - totalDur;
        if (w < 0) w = 0;
        result.metrics.sumWaitingTimes += w;
        result.metrics.patientsCompleted++;
    }
    return result;
}

// ------------------------------------------------------------
// API Handlers
// ------------------------------------------------------------

string handleHealth() {
    return jsonResponse(200, "OK",
        "{\"success\":true,\"application\":\"PRATIKSHA\","
        "\"version\":\"" + Config::VERSION + "\","
        "\"status\":\"running\",\"backend\":\"C++17\"}");
}

string handleTests(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,\"tests\":[";
    for (size_t i = 0; i < st.tests.size(); ++i) {
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(st.tests[i].id) << "\","
          << "\"name\":\"" << escapeJson(st.tests[i].name) << "\","
          << "\"durationMinutes\":" << st.tests[i].durationMinutes << "}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleCentres(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,\"centres\":[";
    for (size_t i = 0; i < st.centres.size(); ++i) {
        const auto& c = st.centres[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"testId\":\"" << escapeJson(c.testId) << "\","
          << "\"duration\":" << c.durationMinutes << ","
          << "\"durationMinutes\":" << c.durationMinutes << ","
          << "\"available\":" << (c.available() ? "true" : "false") << ","
          << "\"currentPatient\":"
          << (c.currentPatient.empty() ? "null" : "\"" + escapeJson(c.currentPatient) + "\"")
          << ",\"completedCount\":" << c.completedCount << "}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleListPatients(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,\"patients\":[";
    for (size_t i = 0; i < st.patients.size(); ++i) {
        const auto& p = st.patients[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(p.id) << "\","
          << "\"name\":\"" << escapeJson(p.name) << "\","
          << "\"emergency\":" << (p.emergency ? "true" : "false") << ","
          << "\"status\":\""
          << (p.status == PatientStatus::WAITING ? "WAITING" : "COMPLETED")
          << "\",\"currentCentre\":\""
          << escapeJson(p.currentCentre) << "\","
          << "\"currentTestId\":\"" << escapeJson(p.currentTestId) << "\","
          << "\"tests\":[";
        bool first = true;
        for (auto& ts : p.testStates) {
            if (ts.status != TestStatus::WAITING) continue;
            if (!first) j << ",";
            first = false;
            j << "\"" << escapeJson(ts.testId) << "\"";
        }
        j << "],\"testsStatus\":[";
        for (size_t k = 0; k < p.testStates.size(); ++k) {
            const auto& ts = p.testStates[k];
            if (k) j << ",";
            j << "{\"testId\":\"" << escapeJson(ts.testId) << "\","
              << "\"status\":\""
              << (ts.status == TestStatus::WAITING ? "WAITING" : "COMPLETED")
              << "\"}";
        }
        j << "]}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleAddPatient(ServerState& st, const string& body) {
    if (body.empty()) return errorResponse(400, "Bad Request", "Request body is empty");
    string id = jsonGetString(body, "id");
    string name = jsonGetString(body, "name");
    vector<string> tests = jsonGetStringArray(body, "tests");
    bool emergency = jsonGetBool(body, "emergency");

    if (id.empty()) return errorResponse(400, "Bad Request", "Patient 'id' is required");
    if (tests.empty()) return errorResponse(400, "Bad Request", "At least one test is required");
    for (auto& t : tests) if (!st.testExists(t))
        return errorResponse(400, "Bad Request", "Unknown test ID: " + t);

    if (!st.addPatient(id, name, tests, emergency)) {
        if (st.hasPatient(id)) return errorResponse(409, "Conflict", "Patient ID already exists");
        return errorResponse(400, "Bad Request", "Could not add patient");
    }

    ostringstream j;
    j << "{\"success\":true,\"patient\":{"
      << "\"id\":\"" << escapeJson(id) << "\","
      << "\"name\":\"" << escapeJson(name) << "\","
      << "\"emergency\":" << (emergency ? "true" : "false") << ",\"tests\":[";
    bool first = true;
    for (auto& t : tests) {
        if (!first) j << ",";
        first = false;
        j << "\"" << escapeJson(t) << "\"";
    }
    j << "]}}";
    return jsonResponse(201, "Created", j.str());
}

string handleDeletePatient(ServerState& st, const string& id) {
    if (id.empty()) return errorResponse(400, "Bad Request", "Patient ID is required");
    if (!st.removePatient(id)) return errorResponse(404, "Not Found", "Patient not found");
    return jsonResponse(200, "OK", "{\"success\":true,\"removed\":\"" + escapeJson(id) + "\"}");
}

string handleGetQueue(const ServerState& st) {
    QueueResult r = computeQueue(st);
    ostringstream j;
    j << "{\"success\":true,"
      << "\"computed\":" << (st.patients.empty() ? "false" : "true") << ","
      << "\"totalTime\":" << r.totalTime << ",";

    j << "\"patients\":[";
    for (size_t i = 0; i < st.patients.size(); ++i) {
        const auto& p = st.patients[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(p.id) << "\","
          << "\"name\":\"" << escapeJson(p.name) << "\","
          << "\"emergency\":" << (p.emergency ? "true" : "false") << "}";
    }
    j << "],";

    j << "\"centres\":[";
    for (size_t i = 0; i < r.centres.size(); ++i) {
        const auto& c = r.centres[i];
        if (i) j << ",";
        int util = r.totalTime > 0 ? (int)((c.busyMinutes * 100.0) / r.totalTime) : 0;
        j << "{\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"duration\":" << c.duration << ","
          << "\"busyMinutes\":" << c.busyMinutes << ","
          << "\"completedCount\":" << c.completedCount << ","
          << "\"utilization\":" << util << "}";
    }
    j << "],";

    j << "\"perPatient\":{";
    bool firstP = true;
    for (auto& kv : r.perPatient) {
        if (!firstP) j << ",";
        firstP = false;
        j << "\"" << escapeJson(kv.first) << "\":[";
        for (size_t i = 0; i < kv.second.size(); ++i) {
            if (i) j << ",";
            j << "{\"testId\":\"" << escapeJson(kv.second[i].testId) << "\","
              << "\"startTime\":" << kv.second[i].startTime << ","
              << "\"duration\":" << kv.second[i].duration << "}";
        }
        j << "]";
    }
    j << "},";

    j << "\"events\":[";
    for (size_t i = 0; i < r.events.size(); ++i) {
        if (i) j << ",";
        j << "{\"time\":" << r.events[i].time
          << ",\"type\":\"" << escapeJson(r.events[i].type) << "\""
          << ",\"centreId\":\"" << escapeJson(r.events[i].centreId) << "\""
          << ",\"patientId\":\"" << escapeJson(r.events[i].patientId) << "\"}";
    }
    j << "],";

    j << "\"metrics\":{"
      << "\"makespan\":" << r.metrics.makespan << ","
      << "\"sumCompletionTimes\":" << r.metrics.sumCompletionTimes << ","
      << "\"sumWaitingTimes\":" << r.metrics.sumWaitingTimes << ","
      << "\"patientsCompleted\":" << r.metrics.patientsCompleted << "}";

    j << "}";
    return jsonResponse(200, "OK", j.str());
}

string handleResetQueue(ServerState& st) {
    st.reset();
    return jsonResponse(200, "OK", "{\"success\":true,\"message\":\"Queue cleared\"}");
}

// ---- GET /api/centres/{id}/next — atomic reservation ----
string handleNextPatient(ServerState& st, const string& centreId) {
    TestingCentre* centre = st.findCentre(centreId);
    if (!centre) return errorResponse(404, "Not Found", "Centre not found");
    if (!centre->available()) return errorResponse(409, "Conflict", "Centre is currently busy");

    // Snapshot current state into a mini-sim so the global scheduler decides
    long maxSeq = st.nextSeq > 0 ? st.nextSeq - 1 : 0;

    vector<SimPatient> sim;
    for (auto& p : st.patients) {
        if (p.status != PatientStatus::WAITING) continue;
        if (!p.currentCentre.empty()) continue;  // in service
        SimPatient sp;
        sp.id = p.id;
        sp.addedSeq = p.addedSeq;
        sp.emergency = p.emergency;
        sp.readyAt = 0;
        for (auto& ts : p.testStates) {
            if (ts.status == TestStatus::WAITING) sp.remaining.push_back(ts.testId);
        }
        if (!sp.remaining.empty()) sim.push_back(sp);
    }

    vector<SimCentre> simC;
    for (auto& c : st.centres) {
        SimCentre sc;
        sc.id = c.id;
        sc.name = c.name;
        sc.testId = c.testId;
        sc.duration = c.durationMinutes;
        if (!c.currentPatient.empty()) {
            sc.currentPatient = c.currentPatient;
            sc.busyUntil = (c.remainingMin > 0 ? (long)c.remainingMin : (long)c.durationMinutes);
        }
        simC.push_back(sc);
    }

    auto assigns = scheduleStep(sim, simC, 0, maxSeq);

    SimAssignment* chosen = nullptr;
    for (auto& a : assigns) if (a.centreId == centreId) { chosen = &a; break; }

    if (!chosen) {
        return jsonResponse(200, "OK",
            "{\"success\":true,\"centreId\":\"" + escapeJson(centreId) +
            "\",\"patient\":null,\"assignmentStatus\":\"NONE\"}");
    }

    Patient* p = st.findPatient(chosen->patientId);
    if (!p) return errorResponse(500, "Internal Server Error", "Patient vanished");

    // Reserve atomically
    p->currentCentre = centreId;
    p->currentTestId = chosen->testId;
    centre->currentPatient = p->id;
    centre->currentTestId = chosen->testId;
    centre->remainingMin = centre->durationMinutes;

    ostringstream j;
    j << "{\"success\":true,\"centreId\":\"" << escapeJson(centreId) << "\","
      << "\"patient\":{"
      << "\"id\":\"" << escapeJson(p->id) << "\","
      << "\"name\":\"" << escapeJson(p->name) << "\","
      << "\"testId\":\"" << escapeJson(chosen->testId) << "\","
      << "\"testName\":\"" << escapeJson(st.testName(chosen->testId)) << "\""
      << "},\"assignmentStatus\":\"ASSIGNED\"}";
    return jsonResponse(200, "OK", j.str());
}

// ---- POST /api/tests/complete ----
string handleCompleteTest(ServerState& st, const string& body) {
    if (body.empty()) return errorResponse(400, "Bad Request", "Request body is empty");
    string patientId = jsonGetString(body, "patientId");
    string testId    = jsonGetString(body, "testId");
    string centreId  = jsonGetString(body, "centreId");

    if (patientId.empty() || testId.empty())
        return errorResponse(400, "Bad Request", "patientId and testId are required");

    Patient* p = st.findPatient(patientId);
    if (!p) return errorResponse(404, "Not Found", "Patient not found");

    PatientTestState* ts = nullptr;
    for (auto& t : p->testStates) {
        if (t.testId == testId && t.status == TestStatus::WAITING) { ts = &t; break; }
    }
    if (!ts)
        return errorResponse(409, "Conflict",
            "Test is not pending (already completed or unknown)");

    // ---- Validation: centre consistency ----
    string actualCentre = p->currentCentre;
    if (!centreId.empty()) {
        TestingCentre* c = st.findCentre(centreId);
        if (!c) return errorResponse(404, "Not Found", "Centre not found");

        if (!actualCentre.empty() && actualCentre != centreId) {
            return errorResponse(409, "Conflict",
                "Patient is assigned to centre " + actualCentre +
                ", not " + centreId);
        }
        if (!c->currentPatient.empty() && c->currentPatient != patientId) {
            return errorResponse(409, "Conflict",
                "Centre " + centreId + " is serving a different patient");
        }
    } else if (!actualCentre.empty()) {
        centreId = actualCentre;  // default to assignment
    }

    // ---- Apply completion ----
    ts->status = TestStatus::COMPLETED;

    if (!centreId.empty()) {
        TestingCentre* c = st.findCentre(centreId);
        if (c) {
            if (c->currentPatient == patientId) {
                c->currentPatient = "";
                c->currentTestId = "";
                c->remainingMin = 0;
            }
            c->completedCount++;
            c->busyMinutes += c->durationMinutes;
        }
    }

    p->currentCentre = "";
    p->currentTestId = "";

    bool allDone = true;
    for (auto& t : p->testStates) if (t.status != TestStatus::COMPLETED) { allDone = false; break; }
    if (allDone) p->status = PatientStatus::COMPLETED;

    ostringstream j;
    j << "{\"success\":true,\"patientId\":\"" << escapeJson(patientId) << "\","
      << "\"testId\":\"" << escapeJson(testId) << "\","
      << "\"completed\":true,\"centreId\":\"" << escapeJson(centreId) << "\"}";
    return jsonResponse(200, "OK", j.str());
}

// ---- Serve frontend ----
string handleHome() {
    FILE* f = fopen("index.html", "rb");
    if (!f) return errorResponse(500, "Internal Server Error", "index.html could not be opened");
    string html;
    char buf[4096];
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        html.append(buf, n);
        if (n < sizeof(buf)) break;
    }
    bool failed = ferror(f);
    fclose(f);
    if (failed) return errorResponse(500, "Internal Server Error", "Failed to read index.html");
    return makeResponse(200, "OK", "text/html; charset=utf-8", html);
}

// ------------------------------------------------------------
// Routing
// ------------------------------------------------------------

string routeRequest(const HttpRequest& req, ServerState& st) {
    const string& m = req.method;
    const string& p = req.path;

    if (m == "GET") {
        if (p == "/")            return handleHome();
        if (p == "/api/health")  return handleHealth();
        if (p == "/api/tests")   return handleTests(st);
        if (p == "/api/centres") return handleCentres(st);
        if (p == "/api/patients") return handleListPatients(st);
        if (p == "/api/queue")   return handleGetQueue(st);

        string rest;
        if (matchPrefix(p, "/api/centres/", rest) &&
            rest.size() > 5 &&
            rest.compare(rest.size() - 5, 5, "/next") == 0) {
            return handleNextPatient(st, rest.substr(0, rest.size() - 5));
        }
    }

    if (m == "POST") {
        if (p == "/api/patients")       return handleAddPatient(st, req.body);
        if (p == "/api/queue/reset")    return handleResetQueue(st);
        if (p == "/api/tests/complete") return handleCompleteTest(st, req.body);
    }

    if (m == "DELETE") {
        string rest;
        if (matchPrefix(p, "/api/patients/", rest))
            return handleDeletePatient(st, rest);
    }

    bool known =
        p == "/" || p == "/api/health" || p == "/api/tests" ||
        p == "/api/centres" || p == "/api/patients" ||
        p == "/api/queue" || p == "/api/tests/complete" ||
        p.rfind("/api/patients/", 0) == 0 ||
        p.rfind("/api/centres/", 0) == 0;

    if (known) return errorResponse(405, "Method Not Allowed", "Method not allowed for this route");
    return errorResponse(404, "Not Found", "Route does not exist");
}

// ------------------------------------------------------------
// Client handler
// ------------------------------------------------------------

void handleClient(int sock, ServerState& st) {
    string raw = readRequest(sock);
    if (raw.empty()) { sendAll(sock, errorResponse(400, "Bad Request", "Empty or unreadable request")); return; }
    HttpRequest req;
    if (!parseRequestLine(raw, req)) {
        sendAll(sock, errorResponse(400, "Bad Request", "Invalid HTTP request line"));
        return;
    }
    size_t q = req.path.find('?');
    if (q != string::npos) req.path = req.path.substr(0, q);
    cout << "[REQUEST] " << req.method << " " << req.path
         << " (" << req.body.size() << " body bytes)" << endl;
    sendAll(sock, routeRequest(req, st));
}

// ------------------------------------------------------------
// Server socket
// ------------------------------------------------------------

int createServerSocket() {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { cerr << "[ERROR] socket(): " << strerror(errno) << endl; return -1; }
    int reuse = 1;
    if (::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        cerr << "[ERROR] setsockopt(): " << strerror(errno) << endl;
        close(s); return -1;
    }
    sockaddr_in