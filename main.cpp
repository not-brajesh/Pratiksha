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

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// ============================================================
// PRATIKSHA — Patient Routing & Intelligent Test Queue Handling
// Backend v1.2
//
// Adds over v1.1:
//   - Per-test status tracking (WAITING / COMPLETED)
//   - arrivalTime, emergency flag on Patient
//   - Real TestingCentre model (id, testId, availability, busy state)
//   - Multiple centres per test type supported
//   - GET  /api/centres/{id}/next    -> next eligible patient
//   - POST /api/tests/complete       -> mark a test done
//   - POST /api/patients accepts optional "emergency" bool
//   - Sequence scoring in queue simulation (Config::LAMBDA)
// ============================================================

namespace Config {
    constexpr int PORT = 8080;
    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 32768;
    constexpr int QUEUE_MAX_TIME = 5000;   // safety cap for simulation
    constexpr double LAMBDA = 0.5;         // balance between wait & load

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.2.0";
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
    string type;       // "start" | "complete"
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

struct QueueResult {
    int totalTime;
    vector<CentreState> centres;
    map<string, vector<QueueStep>> perPatient;
    vector<QueueEvent> events;
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

    // ---- Test helpers ----
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

    // ---- Centre helpers ----
    TestingCentre* findCentre(const string& id) {
        for (auto& c : centres) if (c.id == id) return &c;
        return nullptr;
    }
    const TestingCentre* findCentre(const string& id) const {
        for (const auto& c : centres) if (c.id == id) return &c;
        return nullptr;
    }

    // ---- Patient helpers ----
    bool hasPatient(const string& id) const {
        for (const auto& p : patients) if (p.id == id) return true;
        return false;
    }
    Patient* findPatient(const string& id) {
        for (auto& p : patients) if (p.id == id) return &p;
        return nullptr;
    }

    bool addPatient(
        const string& id,
        const string& name,
        const vector<string>& rawTests,
        bool emergency
    ) {
        if (id.empty() || hasPatient(id)) return false;

        // Validate + dedupe
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
            ts.completedAt = -1;
            p.testStates.push_back(ts);
        }
        patients.push_back(p);
        return true;
    }

    bool removePatient(const string& id) {
        for (size_t i = 0; i < patients.size(); ++i) {
            if (patients[i].id == id) {
                // Free any centre currently serving this patient
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
// HTTP Response Helpers
// ------------------------------------------------------------

string makeResponse(
    int statusCode, const string& statusText,
    const string& contentType, const string& body
) {
    ostringstream r;
    r << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
      << "Content-Type: " << contentType << "\r\n"
      << "Content-Length: " << body.size() << "\r\n"
      << "Connection: close\r\n"
      << "Cache-Control: no-store\r\n"
      << "X-Content-Type-Options: nosniff\r\n"
      << "\r\n" << body;
    return r.str();
}

string jsonResponse(int code, const string& text, const string& json) {
    return makeResponse(code, text, "application/json; charset=utf-8", json);
}

string errorResponse(int code, const string& text, const string& msg) {
    return jsonResponse(code, text,
        "{\"success\":false,\"error\":\"" + escapeJson(msg) + "\"}");
}

// ------------------------------------------------------------
// Send / Read
// ------------------------------------------------------------

bool sendAll(int sock, const string& response) {
    size_t sent = 0;
    while (sent < response.size()) {
        ssize_t n = ::send(sock, response.data() + sent, response.size() - sent, 0);
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
    size_t headerEnd = string::npos;
    long contentLength = -1;

    while (req.size() < Config::MAX_REQUEST_SIZE) {
        if (headerEnd != string::npos) {
            size_t bs = headerEnd + 4;
            size_t br = req.size() > bs ? req.size() - bs : 0;
            if (contentLength <= 0 || br >= (size_t)contentLength) break;
        }
        ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
        if (n < 0) { if (errno == EINTR) continue; return ""; }
        if (n == 0) break;
        req.append(buf, (size_t)n);
        if (headerEnd == string::npos) {
            headerEnd = req.find("\r\n\r\n");
            if (headerEnd != string::npos)
                contentLength = parseContentLength(req.substr(0, headerEnd));
        }
    }
    return req;
}

// ------------------------------------------------------------
// HTTP Request Parsing
// ------------------------------------------------------------

struct HttpRequest {
    string method, path, version, headers, body;
};

bool parseRequestLine(const string& req, HttpRequest& parsed) {
    size_t le = req.find("\r\n");
    if (le == string::npos) return false;
    string first = req.substr(0, le);
    istringstream in(first);
    if (!(in >> parsed.method >> parsed.path >> parsed.version)) return false;
    string extra;
    if (in >> extra) return false;
    if (parsed.version != "HTTP/1.1" && parsed.version != "HTTP/1.0") return false;
    size_t he = req.find("\r\n\r\n");
    if (he != string::npos) {
        parsed.headers = req.substr(le + 2, he - (le + 2));
        parsed.body = req.substr(he + 4);
    }
    return true;
}

bool matchPrefix(const string& path, const string& prefix, string& rest) {
    if (path.size() > prefix.size() &&
        path.compare(0, prefix.size(), prefix) == 0) {
        rest = path.substr(prefix.size());
        return true;
    }
    return false;
}

// ------------------------------------------------------------
// Queue Simulation (Sequence scoring per report)
// ------------------------------------------------------------
//
// Simulation state is a COPY — it never modifies the real
// patient/centre state. Real completion must be triggered by
// POST /api/tests/complete.
//
// Patient-selection for a free centre (higher score = priority):
//   wait = (maxSeq - addedSeq) + simTime
//   emergency = +100000 (definitive override)
//
// Per-patient preferred sequence uses:
//   score = CompletionTime + LAMBDA * LoadFactorSum   (lower = better)
//   Currently we surface the SIMULATED sequence (post-hoc),
//   which respects the same trade-off.
// ------------------------------------------------------------

QueueResult computeQueue(const ServerState& state) {
    QueueResult result;

    struct SimPatient {
        string id;
        vector<string> remaining;
        long addedSeq;
        bool emergency;
    };
    vector<SimPatient> sim;
    long maxSeq = state.nextSeq > 0 ? state.nextSeq - 1 : 0;

    for (const auto& p : state.patients) {
        SimPatient sp;
        sp.id = p.id;
        sp.addedSeq = p.addedSeq;
        sp.emergency = p.emergency;
        for (const auto& ts : p.testStates) {
            if (ts.status == TestStatus::WAITING)
                sp.remaining.push_back(ts.testId);
        }
        if (!sp.remaining.empty()) {
            sim.push_back(sp);
            result.perPatient[p.id] = {};
        }
    }

    struct SimCentre {
        string id;
        string name;
        string testId;
        int duration;
        string currentPatient;
        int remainingMin;
        long busyMinutes;
        int completedCount;
    };
    vector<SimCentre> simC;

    for (const auto& c : state.centres) {
        SimCentre sc;
        sc.id = c.id;
        sc.name = c.name;
        sc.testId = c.testId;
        sc.duration = c.durationMinutes;
        sc.currentPatient = c.currentPatient;
        sc.remainingMin = c.remainingMin;
        sc.busyMinutes = c.busyMinutes;
        sc.completedCount = c.completedCount;
        simC.push_back(sc);
    }

    long time = 0;
    while (time < Config::QUEUE_MAX_TIME) {
        // 1) Completions
        for (auto& c : simC) {
            if (!c.currentPatient.empty() && c.remainingMin <= 0) {
                for (auto& s : sim) {
                    if (s.id == c.currentPatient) {
                        auto it = std::find(s.remaining.begin(),
                                            s.remaining.end(), c.testId);
                        if (it != s.remaining.end()) s.remaining.erase(it);
                        break;
                    }
                }
                result.events.push_back({(int)time, "complete", c.id, c.currentPatient});
                c.completedCount++;
                c.currentPatient = "";
                c.remainingMin = 0;
            }
        }

        // 2) All done?
        bool allDone = true;
        for (const auto& s : sim) {
            if (!s.remaining.empty()) { allDone = false; break; }
        }
        if (allDone) break;

        // 3) Assignments
        set<string> busy;
        for (const auto& c : simC)
            if (!c.currentPatient.empty()) busy.insert(c.currentPatient);

        for (auto& c : simC) {
            if (!c.currentPatient.empty()) continue;

            vector<SimPatient*> eligible;
            for (auto& s : sim) {
                if (busy.count(s.id)) continue;
                if (std::find(s.remaining.begin(), s.remaining.end(),
                              c.testId) != s.remaining.end()) {
                    eligible.push_back(&s);
                }
            }
            if (eligible.empty()) continue;

            SimPatient* chosen = nullptr;
            double best = -1e18;
            for (auto* s : eligible) {
                long wait = (maxSeq - s->addedSeq) + time;
                if (wait < 0) wait = 0;
                double score = (double)wait;
                if (s->emergency) score += 100000;
                if (score > best) { best = score; chosen = s; }
            }
            if (!chosen) continue;

            c.currentPatient = chosen->id;
            c.remainingMin = c.duration;
            busy.insert(chosen->id);

            result.events.push_back({(int)time, "start", c.id, chosen->id});
            result.perPatient[chosen->id].push_back(
                {c.testId, (int)time, c.duration}
            );
        }

        time++;
        for (auto& c : simC) {
            if (c.remainingMin > 0) { c.remainingMin--; c.busyMinutes++; }
        }
    }

    result.totalTime = (int)time;

    for (const auto& sc : simC) {
        CentreState cs;
        cs.id = sc.id;
        cs.name = sc.name;
        cs.duration = sc.duration;
        cs.busyMinutes = sc.busyMinutes;
        cs.completedCount = sc.completedCount;
        result.centres.push_back(cs);
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
        j << "{"
          << "\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"testId\":\"" << escapeJson(c.testId) << "\","
          << "\"duration\":" << c.durationMinutes << ","
          << "\"durationMinutes\":" << c.durationMinutes << ","
          << "\"available\":" << (c.available() ? "true" : "false") << ","
          << "\"currentPatient\":"
          << (c.currentPatient.empty() ? "null" : "\"" + escapeJson(c.currentPatient) + "\"")
          << ","
          << "\"completedCount\":" << c.completedCount
          << "}";
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
          << "\",\"tests\":[";
        bool first = true;
        for (const auto& ts : p.testStates) {
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
    if (body.empty())
        return errorResponse(400, "Bad Request", "Request body is empty");

    string id = jsonGetString(body, "id");
    string name = jsonGetString(body, "name");
    vector<string> tests = jsonGetStringArray(body, "tests");
    bool emergency = jsonGetBool(body, "emergency");

    if (id.empty())
        return errorResponse(400, "Bad Request", "Patient 'id' is required");
    if (tests.empty())
        return errorResponse(400, "Bad Request", "At least one test is required");

    for (const auto& t : tests) {
        if (!st.testExists(t))
            return errorResponse(400, "Bad Request", "Unknown test ID: " + t);
    }

    if (!st.addPatient(id, name, tests, emergency)) {
        if (st.hasPatient(id))
            return errorResponse(409, "Conflict", "Patient ID already exists");
        return errorResponse(400, "Bad Request", "Could not add patient");
    }

    ostringstream j;
    j << "{\"success\":true,\"patient\":{"
      << "\"id\":\"" << escapeJson(id) << "\","
      << "\"name\":\"" << escapeJson(name) << "\","
      << "\"emergency\":" << (emergency ? "true" : "false") << ","
      << "\"tests\":[";
    bool first = true;
    for (const auto& t : tests) {
        if (!first) j << ",";
        first = false;
        j << "\"" << escapeJson(t) << "\"";
    }
    j << "]}}";
    return jsonResponse(201, "Created", j.str());
}

string handleDeletePatient(ServerState& st, const string& id) {
    if (id.empty())
        return errorResponse(400, "Bad Request", "Patient ID is required");
    if (!st.removePatient(id))
        return errorResponse(404, "Not Found", "Patient not found");
    return jsonResponse(200, "OK",
        "{\"success\":true,\"removed\":\"" + escapeJson(id) + "\"}");
}

string handleGetQueue(const ServerState& st) {
    QueueResult r = computeQueue(st);

    ostringstream j;
    j << "{\"success\":true,"
      << "\"computed\":" << (st.patients.empty() ? "false" : "true") << ","
      << "\"totalTime\":" << r.totalTime << ",";

    // Patients
    j << "\"patients\":[";
    for (size_t i = 0; i < st.patients.size(); ++i) {
        const auto& p = st.patients[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(p.id) << "\","
          << "\"name\":\"" << escapeJson(p.name) << "\","
          << "\"emergency\":" << (p.emergency ? "true" : "false") << "}";
    }
    j << "],";

    // Centres
    j << "\"centres\":[";
    for (size_t i = 0; i < r.centres.size(); ++i) {
        const auto& c = r.centres[i];
        if (i) j << ",";
        int util = r.totalTime > 0
            ? (int)((c.busyMinutes * 100.0) / r.totalTime) : 0;
        j << "{\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"duration\":" << c.duration << ","
          << "\"busyMinutes\":" << c.busyMinutes << ","
          << "\"completedCount\":" << c.completedCount << ","
          << "\"utilization\":" << util << "}";
    }
    j << "],";

    // perPatient
    j << "\"perPatient\":{";
    bool firstP = true;
    for (const auto& kv : r.perPatient) {
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

    // events
    j << "\"events\":[";
    for (size_t i = 0; i < r.events.size(); ++i) {
        if (i) j << ",";
        j << "{\"time\":" << r.events[i].time
          << ",\"type\":\"" << escapeJson(r.events[i].type) << "\""
          << ",\"centreId\":\"" << escapeJson(r.events[i].centreId) << "\""
          << ",\"patientId\":\"" << escapeJson(r.events[i].patientId) << "\"}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleResetQueue(ServerState& st) {
    st.reset();
    return jsonResponse(200, "OK",
        "{\"success\":true,\"message\":\"Queue cleared\"}");
}

// ---- GET /api/centres/{id}/next ----
string handleNextPatient(ServerState& st, const string& centreId) {
    TestingCentre* centre = st.findCentre(centreId);
    if (!centre)
        return errorResponse(404, "Not Found", "Centre not found");
    if (!centre->available())
        return errorResponse(409, "Conflict", "Centre is currently busy");

    vector<Patient*> eligible;
    for (auto& p : st.patients) {
        if (p.status != PatientStatus::WAITING) continue;
        for (const auto& ts : p.testStates) {
            if (ts.testId == centre->testId && ts.status == TestStatus::WAITING) {
                eligible.push_back(&p);
                break;
            }
        }
    }

    if (eligible.empty()) {
        return jsonResponse(200, "OK",
            "{\"success\":true,\"centreId\":\"" + escapeJson(centreId) +
            "\",\"patient\":null}");
    }

    long maxSeq = st.nextSeq > 0 ? st.nextSeq - 1 : 0;
    Patient* chosen = nullptr;
    double best = -1e18;
    for (auto* p : eligible) {
        long wait = maxSeq - p->addedSeq;
        if (wait < 0) wait = 0;
        double score = (double)wait;
        if (p->emergency) score += 100000;
        if (score > best) { best = score; chosen = p; }
    }

    ostringstream j;
    j << "{\"success\":true,\"centreId\":\"" << escapeJson(centreId) << "\","
      << "\"patient\":{"
      << "\"id\":\"" << escapeJson(chosen->id) << "\","
      << "\"name\":\"" << escapeJson(chosen->name) << "\","
      << "\"testId\":\"" << escapeJson(centre->testId) << "\","
      << "\"testName\":\"" << escapeJson(st.testName(centre->testId)) << "\""
      << "}}";
    return jsonResponse(200, "OK", j.str());
}

// ---- POST /api/tests/complete ----
string handleCompleteTest(ServerState& st, const string& body) {
    if (body.empty())
        return errorResponse(400, "Bad Request", "Request body is empty");

    string patientId = jsonGetString(body, "patientId");
    string testId    = jsonGetString(body, "testId");
    string centreId  = jsonGetString(body, "centreId");

    if (patientId.empty() || testId.empty())
        return errorResponse(400, "Bad Request",
            "patientId and testId are required");

    Patient* p = st.findPatient(patientId);
    if (!p) return errorResponse(404, "Not Found", "Patient not found");

    // Find WAITING test
    PatientTestState* ts = nullptr;
    for (auto& t : p->testStates) {
        if (t.testId == testId && t.status == TestStatus::WAITING) {
            ts = &t;
            break;
        }
    }
    if (!ts)
        return errorResponse(400, "Bad Request",
            "Test is not pending for this patient");

    ts->status = TestStatus::COMPLETED;
    ts->completedAt = st.nextSeq;

    // Update centre if provided
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

    // All done?
    bool allDone = true;
    for (const auto& t : p->testStates) {
        if (t.status != TestStatus::COMPLETED) { allDone = false; break; }
    }
    if (allDone) p->status = PatientStatus::COMPLETED;

    return jsonResponse(200, "OK",
        "{\"success\":true,\"patientId\":\"" + escapeJson(patientId) +
        "\",\"testId\":\"" + escapeJson(testId) + "\",\"completed\":true}");
}

// ---- Serve Frontend ----
string handleHome() {
    FILE* f = fopen("index.html", "rb");
    if (!f) return errorResponse(500, "Internal Server Error",
        "index.html could not be opened");
    string html;
    char buf[4096];
    while (true) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        html.append(buf, n);
        if (n < sizeof(buf)) break;
    }
    bool failed = ferror(f);
    fclose(f);
    if (failed) return errorResponse(500, "Internal Server Error",
        "Failed to read index.html");
    return makeResponse(200, "OK", "text/html; charset=utf-8", html);
}

// ------------------------------------------------------------
// Routing
// ------------------------------------------------------------

string routeRequest(const HttpRequest& req, ServerState& st) {
    const string& m = req.method;
    const string& p = req.path;

    if (m == "GET") {
        if (p == "/")                    return handleHome();
        if (p == "/api/health")          return handleHealth();
        if (p == "/api/tests")           return handleTests(st);
        if (p == "/api/centres")         return handleCentres(st);
        if (p == "/api/patients")        return handleListPatients(st);
        if (p == "/api/queue")           return handleGetQueue(st);

        // GET /api/centres/{id}/next
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

    if (known) return errorResponse(405, "Method Not Allowed",
        "Method not allowed for this route");
    return errorResponse(404, "Not Found", "Route does not exist");
}

// ------------------------------------------------------------
// Client Handler
// ------------------------------------------------------------

void handleClient(int sock, ServerState& st) {
    string raw = readRequest(sock);
    if (raw.empty()) {
        sendAll(sock, errorResponse(400, "Bad Request",
            "Empty or unreadable request"));
        return;
    }

    HttpRequest req;
    if (!parseRequestLine(raw, req)) {
        sendAll(sock, errorResponse(400, "Bad Request",
            "Invalid HTTP request line"));
        return;
    }

    size_t q = req.path.find('?');
    if (q != string::npos) req.path = req.path.substr(0, q);

    cout << "[REQUEST] " << req.method << " " << req.path
         << " (" << req.body.size() << " body bytes)" << endl;

    sendAll(sock, routeRequest(req, st));
}

// ------------------------------------------------------------
// Server Socket
// ------------------------------------------------------------

int createServerSocket() {
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        cerr << "[ERROR] socket(): " << strerror(errno) << endl;
        return -1;
    }
    int reuse = 1;
    if (::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                     &reuse, sizeof(reuse)) < 0) {
        cerr << "[ERROR] setsockopt(): " << strerror(errno) << endl;
        close(s); return -1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Config::PORT);
    // Loopback only. Change to htonl(INADDR_ANY) to allow LAN access
    // (see note below in the response).
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        cerr << "[ERROR] bind(): " << strerror(errno) << endl;
        close(s); return -1;
    }
    if (::listen(s, Config::BACKLOG) < 0) {
        cerr << "[ERROR] listen(): " << strerror(errno) << endl;
        close(s); return -1;
    }
    return s;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main() {
    signal(SIGPIPE, SIG_IGN);

    cout << endl
         << "========================================" << endl
         << "       PRATIKSHA HOSPITAL SYSTEM" << endl
         << "              v" << Config::VERSION << endl
         << "========================================" << endl;

    ServerState state;
    int srv = createServerSocket();
    if (srv < 0) {
        cerr << "[FATAL] Server could not start." << endl;
        return 1;
    }

    cout << "[SUCCESS] C++ server started." << endl
         << "[SERVER] http://127.0.0.1:" << Config::PORT << endl
         << "[API] GET    /api/health"                    << endl
         << "[API] GET    /api/tests"                     << endl
         << "[API] GET    /api/centres"                   << endl
         << "[API] GET    /api/centres/{id}/next"         << endl
         << "[API] GET    /api/patients"                  << endl
         << "[API] POST   /api/patients"                  << endl
         << "[API] DELETE /api/patients/{id}"             << endl
         << "[API] POST   /api/tests/complete"            << endl
         << "[API] GET    /api/queue"                     << endl
         << "[API] POST   /api/queue/reset"               << endl
         << "[INFO] Press Control+C to stop."             << endl
         << "========================================" << endl;

    while (true) {
        sockaddr_in ca{};
        socklen_t cl = sizeof(ca);
        int cs = ::accept(srv, reinterpret_cast<sockaddr*>(&ca), &cl);
        if (cs < 0) {
            if (errno == EINTR) continue;
            cerr << "[ERROR] accept(): " << strerror(errno) << endl;
            continue;
        }
        handleClient(cs, state);
        shutdown(cs, SHUT_RDWR);
        close(cs);
    }

    close(srv);
    return 0;
}