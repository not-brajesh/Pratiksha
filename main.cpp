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
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// ============================================================
// PRATIKSHA — Patient Routing & Intelligent Test Queue Handling
// Backend v1.7.2
//
// Changes over v1.7.1:
//   [Auto-mode fix] handleNextPatient() now restricts its
//   scheduling simulation to ONLY the requested centre.
//   Previously, all centres were fed into scheduleStep(), so a
//   "next patient" request for centre X could consume a patient
//   that the scheduler had already matched to another centre Y —
//   leaving X with patient:null even when an eligible patient
//   existed. With `if (c.id != centreId) continue;` in the
//   sim-centres build loop, the scheduler considers only X's
//   eligibility, guaranteeing a match whenever one exists.
//
//   [PORT-config] PORT is now read from the PORT environment
//   variable (validated 1–65535). Falls back to 8080 when unset
//   or invalid. Backlog / max-request / timing settings are
//   unchanged.
//
// Existing backend features (unchanged since v1.7.0):
//   [Step 1] Auto-generated patient IDs (P001, P002, …)
//   [Step 2] Single machine per testing centre (A1/B1/C1/D1)
//   [Step 3] JSON config import (POST /api/configuration)
//   [Step 4] Live centre timing: steady_clock timestamps,
//            /api/centres → {status, elapsedSeconds,
//            remainingSeconds}, AWAITING_CONFIRMATION state
//   [Step 5] Persistent event history (GET /api/events)
// ============================================================

namespace Config {
    // [PORT-config] Read port from env var PORT with validation,
    // falling back to 8080 when unset / invalid.
    inline int getPort() {
        const char* envPort = std::getenv("PORT");

        if (envPort != nullptr) {
            int port = std::atoi(envPort);

            if (port > 0 && port <= 65535) {
                return port;
            }
        }

        return 8080;
    }

    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 131072; // 128KB
    constexpr int QUEUE_MAX_TIME = 5000;
    constexpr long INF_TIME = 1000000000L;
    constexpr double LAMBDA = 0.5;
    constexpr int MOVE_TIME_MINUTES = 3;
    constexpr size_t MAX_EVENT_LOG = 500;

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.7.2";
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

    long long startedAtMs = 0;
    long long expectedCompletionMs = 0;

    bool available() const { return currentPatient.empty(); }
};

struct EventLogEntry {
    int id = 0;
    long long wallMs = 0;
    string type;                     // "start" | "complete"
    string patientId;
    string patientName;
    string testId;
    string testName;
    string centreId;
    string centreName;
    int durationMinutes = 0;
    long long actualElapsedSeconds = 0;
};

struct QueueStep {
    string testId;
    string centreId;
    int startTime;
    int duration;
};

struct QueueEvent {
    int time;
    string type;
    string centreId;
    string patientId;
    string testId;
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
    long sumMovementTimes = 0;
    int testsCompleted = 0;
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
// Time helpers
// ------------------------------------------------------------

long long currentSteadyMs() {
    using namespace chrono;
    return duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}

long long currentWallMs() {
    using namespace chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

// Returns int seconds remaining; 0 if not applicable.
// Frontend treats this as a snapshot at fetch time and adds
// its own client-side drift.
int centreRemainingSeconds(const TestingCentre& c) {
    if (c.currentPatient.empty()) return 0;
    if (c.expectedCompletionMs <= 0) return 0;
    long long now = currentSteadyMs();
    if (now >= c.expectedCompletionMs) return 0;
    return (int)((c.expectedCompletionMs - now) / 1000);
}

// Returns int seconds elapsed since test start; 0 if not applicable.
// Frontend adds client-side drift on top of this snapshot.
int centreElapsedSeconds(const TestingCentre& c) {
    if (c.currentPatient.empty()) return 0;
    if (c.startedAtMs <= 0) return 0;
    long long now = currentSteadyMs();
    if (now < c.startedAtMs) return 0;
    return (int)((now - c.startedAtMs) / 1000);
}

string centreStatusString(const TestingCentre& c) {
    if (c.currentPatient.empty()) return "AVAILABLE";
    if (c.expectedCompletionMs <= 0) return "BUSY";
    if (currentSteadyMs() >= c.expectedCompletionMs) return "AWAITING_CONFIRMATION";
    return "BUSY";
}

// ------------------------------------------------------------
// Server State
// ------------------------------------------------------------

class ServerState {
public:
    vector<DiagnosticTest> tests;
    vector<TestingCentre> centres;
    vector<Patient> patients;
    long nextSeq = 0;

    int nextPatientNumber = 1;

    vector<EventLogEntry> eventLog;
    int nextEventId = 1;

    ServerState() {
        tests = {
            {"A", "X-Ray", 10},
            {"B", "Blood Test", 5},
            {"C", "Ultrasound", 20},
            {"D", "CT Scan", 30}
        };
        centres = {
            {"A1", "X-Ray Room", "A", 10},
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

    string peekNextPatientId() const {
        ostringstream id;
        id << "P";
        id.width(3);
        id.fill('0');
        id << nextPatientNumber;
        return id.str();
    }

    string generatePatientId() {
        ostringstream id;
        id << "P";
        id.width(3);
        id.fill('0');
        id << nextPatientNumber++;
        return id.str();
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
                        c.startedAtMs = 0;
                        c.expectedCompletionMs = 0;
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
        nextPatientNumber = 1;
        eventLog.clear();
        for (auto& c : centres) {
            c.currentPatient = "";
            c.currentTestId = "";
            c.remainingMin = 0;
            c.busyMinutes = 0;
            c.completedCount = 0;
            c.startedAtMs = 0;
            c.expectedCompletionMs = 0;
        }
    }

    void logEvent(const string& type,
                  const string& patientId, const string& patientName,
                  const string& testId, const string& testName,
                  const string& centreId, const string& centreName,
                  int durationMinutes,
                  long long actualElapsedSeconds) {
        EventLogEntry e;
        e.id = nextEventId++;
        e.wallMs = currentWallMs();
        e.type = type;
        e.patientId = patientId;
        e.patientName = patientName;
        e.testId = testId;
        e.testName = testName;
        e.centreId = centreId;
        e.centreName = centreName;
        e.durationMinutes = durationMinutes;
        e.actualElapsedSeconds = actualElapsedSeconds;
        eventLog.push_back(std::move(e));

        if (eventLog.size() > Config::MAX_EVENT_LOG) {
            size_t drop = eventLog.size() - Config::MAX_EVENT_LOG;
            eventLog.erase(eventLog.begin(), eventLog.begin() + drop);
        }
    }
};

// ------------------------------------------------------------
// JSON escaping (for output)
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

// ------------------------------------------------------------
// Minimal self-contained JSON parser
// ------------------------------------------------------------

struct JsonValue {
    enum Type { NUL, BOOL, NUM, STR, ARR, OBJ } type = NUL;
    bool bval = false;
    double nval = 0;
    string sval;
    vector<JsonValue> arr;
    map<string, JsonValue> obj;

    bool isObj() const { return type == OBJ; }
    bool isArr() const { return type == ARR; }
    bool isStr() const { return type == STR; }
    bool isNum() const { return type == NUM; }
    bool isBool() const { return type == BOOL; }
    bool has(const string& k) const {
        return type == OBJ && obj.find(k) != obj.end();
    }
    const JsonValue& at(const string& k) const {
        return obj.at(k);
    }
    string asStr(const string& fallback = "") const {
        return type == STR ? sval : fallback;
    }
};

class JsonParser {
public:
    JsonParser(const string& text) : s(text), pos(0) {}
    bool parse(JsonValue& out) {
        skipWs();
        if (!parseValue(out)) return false;
        skipWs();
        return pos == s.size();
    }
private:
    const string& s;
    size_t pos;

    void skipWs() {
        while (pos < s.size() && isspace((unsigned char)s[pos])) pos++;
    }
    bool parseValue(JsonValue& out) {
        skipWs();
        if (pos >= s.size()) return false;
        char c = s[pos];
        if (c == '{') return parseObject(out);
        if (c == '[') return parseArray(out);
        if (c == '"') return parseString(out);
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == 'n') return parseNull(out);
        return parseNumber(out);
    }
    bool parseString(JsonValue& out) {
        if (pos >= s.size() || s[pos] != '"') return false;
        pos++;
        out.type = JsonValue::STR;
        out.sval.clear();
        while (pos < s.size() && s[pos] != '"') {
            char c = s[pos];
            if (c == '\\') {
                pos++;
                if (pos >= s.size()) return false;
                char e = s[pos];
                switch (e) {
                    case 'n': out.sval += '\n'; break;
                    case 't': out.sval += '\t'; break;
                    case 'r': out.sval += '\r'; break;
                    case '"': out.sval += '"'; break;
                    case '\\': out.sval += '\\'; break;
                    case '/': out.sval += '/'; break;
                    case 'b': out.sval += '\b'; break;
                    case 'f': out.sval += '\f'; break;
                    case 'u': {
                        if (pos + 4 >= s.size()) return false;
                        unsigned cp = 0;
                        for (int k = 1; k <= 4; ++k) {
                            char h = s[pos + k];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= (h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                            else return false;
                        }
                        pos += 4;
                        if (cp < 0x80) out.sval += (char)cp;
                        else if (cp < 0x800) {
                            out.sval += (char)(0xC0 | (cp >> 6));
                            out.sval += (char)(0x80 | (cp & 0x3F));
                        } else {
                            out.sval += (char)(0xE0 | (cp >> 12));
                            out.sval += (char)(0x80 | ((cp >> 6) & 0x3F));
                            out.sval += (char)(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default: return false;
                }
                pos++;
            } else {
                out.sval += c;
                pos++;
            }
        }
        if (pos >= s.size()) return false;
        pos++;
        return true;
    }
    bool parseNumber(JsonValue& out) {
        size_t start = pos;
        if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) pos++;
        while (pos < s.size()) {
            char c = s[pos];
            if (isdigit((unsigned char)c) || c == '.' || c == 'e' ||
                c == 'E' || c == '+' || c == '-') {
                pos++;
            } else break;
        }
        if (pos == start) return false;
        try {
            out.nval = stod(s.substr(start, pos - start));
            out.type = JsonValue::NUM;
            return true;
        } catch (...) { return false; }
    }
    bool parseBool(JsonValue& out) {
        if (s.compare(pos, 4, "true") == 0) { pos += 4; out.type = JsonValue::BOOL; out.bval = true; return true; }
        if (s.compare(pos, 5, "false") == 0) { pos += 5; out.type = JsonValue::BOOL; out.bval = false; return true; }
        return false;
    }
    bool parseNull(JsonValue& out) {
        if (s.compare(pos, 4, "null") == 0) { pos += 4; out.type = JsonValue::NUL; return true; }
        return false;
    }
    bool parseArray(JsonValue& out) {
        if (pos >= s.size() || s[pos] != '[') return false;
        pos++;
        out.type = JsonValue::ARR;
        skipWs();
        if (pos < s.size() && s[pos] == ']') { pos++; return true; }
        while (true) {
            JsonValue v;
            if (!parseValue(v)) return false;
            out.arr.push_back(std::move(v));
            skipWs();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') { pos++; continue; }
            if (s[pos] == ']') { pos++; return true; }
            return false;
        }
    }
    bool parseObject(JsonValue& out) {
        if (pos >= s.size() || s[pos] != '{') return false;
        pos++;
        out.type = JsonValue::OBJ;
        skipWs();
        if (pos < s.size() && s[pos] == '}') { pos++; return true; }
        while (true) {
            skipWs();
            JsonValue key;
            if (!parseString(key)) return false;
            skipWs();
            if (pos >= s.size() || s[pos] != ':') return false;
            pos++;
            JsonValue val;
            if (!parseValue(val)) return false;
            out.obj[key.sval] = std::move(val);
            skipWs();
            if (pos >= s.size()) return false;
            if (s[pos] == ',') { pos++; continue; }
            if (s[pos] == '}') { pos++; return true; }
            return false;
        }
    }
};

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
    char buf[8192];
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
// Simulation types
// ------------------------------------------------------------

struct SimPatient {
    string id;
    vector<string> remaining;
    long addedSeq = 0;
    bool emergency = false;

    int testsCompleted = 0;
    bool inService = false;
    string currentCentreId = "";
    long readyAt = 0;

    long accumulatedWait = 0;
    long accumulatedMovement = 0;
    long lastCompletionTime = 0;
};

struct SimCentre {
    string id;
    string name;
    string testId;
    int duration = 0;
    string currentPatient = "";
    long busyUntil = 0;
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

double computePriority(
    const SimPatient& p,
    const SimCentre& c,
    long currentTime,
    long /*maxSeq*/,
    int waitingForTest,
    int centresForTest
) {
    double actualWait = (double)(currentTime - p.readyAt);
    if (actualWait < 0) actualWait = 0;

    double score = 0.0;
    if (p.emergency) score += 1e9;
    score += actualWait * 100.0;
    if (c.duration > 0) score += 1000.0 / c.duration;
    if (centresForTest > 0) {
        score -= Config::LAMBDA * ((double)waitingForTest / (double)centresForTest);
    }
    score -= (double)p.addedSeq * 1e-6;
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

    std::stable_sort(candidates.begin(), candidates.end(),
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
// computeQueue — event-based simulation
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

    long long nowMs = currentSteadyMs();

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
            sc.currentPatient = c.currentPatient;
            if (c.expectedCompletionMs > 0) {
                if (nowMs >= c.expectedCompletionMs) {
                    sc.busyUntil = 0;
                } else {
                    long long remMs = c.expectedCompletionMs - nowMs;
                    long remMin = (long)((remMs + 59999) / 60000);
                    if (remMin < 1) remMin = 1;
                    sc.busyUntil = remMin;
                }
            } else {
                sc.busyUntil = (c.remainingMin > 0
                                ? (long)c.remainingMin
                                : (long)c.durationMinutes);
            }
        }
        simC.push_back(sc);
    }

    for (const auto& c : simC) {
        if (c.currentPatient.empty()) continue;
        for (auto& p : sim) {
            if (p.id == c.currentPatient) {
                p.inService = true;
                p.currentCentreId = c.id;
                p.readyAt = c.busyUntil;
                break;
            }
        }
    }

    for (const auto& sc : simC) {
        if (sc.currentPatient.empty()) continue;
        if (!result.perPatient.count(sc.currentPatient)) continue;
        long startTime = sc.busyUntil - sc.duration;
        result.perPatient[sc.currentPatient].push_back({
            sc.testId, sc.id, (int)startTime, sc.duration
        });
    }

    long time = 0;
    while (time < Config::QUEUE_MAX_TIME) {
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
                        p.lastCompletionTime = completeTime;
                        if (!p.remaining.empty()) {
                            p.readyAt = completeTime + Config::MOVE_TIME_MINUTES;
                            p.accumulatedMovement += Config::MOVE_TIME_MINUTES;
                        } else {
                            p.readyAt = completeTime;
                        }
                        p.currentCentreId = "";
                        break;
                    }
                }
                result.events.push_back({
                    (int)completeTime, "complete", c.id, c.currentPatient, c.testId
                });
                c.completedCount++;
                c.busyMinutes += c.duration;
                c.currentPatient = "";
                c.busyUntil = 0;
            }
        }

        bool allDone = true;
        for (auto& p : sim) if (!p.remaining.empty()) { allDone = false; break; }
        if (allDone) break;

        auto assigns = scheduleStep(sim, simC, time, maxSeq);
        for (auto& a : assigns) {
            SimCentre* c = nullptr;
            SimPatient* p = nullptr;
            for (auto& x : simC) if (x.id == a.centreId) { c = &x; break; }
            for (auto& x : sim) if (x.id == a.patientId) { p = &x; break; }
            if (!c || !p) continue;

            if (time >= p->readyAt) {
                p->accumulatedWait += (time - p->readyAt);
            }

            c->currentPatient = p->id;
            c->busyUntil = time + c->duration;
            p->inService = true;
            p->currentCentreId = c->id;

            result.events.push_back({(int)time, "start", c->id, p->id, c->testId});
            result.perPatient[p->id].push_back({
                c->testId, c->id, (int)time, c->duration
            });
        }

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
    result.metrics.makespan = result.totalTime;

    for (const auto& sc : simC) {
        CentreState cs;
        cs.id = sc.id;
        cs.name = sc.name;
        cs.duration = sc.duration;
        cs.busyMinutes = sc.busyMinutes;
        cs.completedCount = sc.completedCount;
        result.centres.push_back(cs);
    }

    for (auto& p : sim) {
        result.metrics.sumWaitingTimes += p.accumulatedWait;
        result.metrics.sumMovementTimes += p.accumulatedMovement;
        result.metrics.testsCompleted += p.testsCompleted;

        if (p.remaining.empty()) {
            result.metrics.sumCompletionTimes += p.lastCompletionTime;
            result.metrics.patientsCompleted++;
        }
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
        "\"status\":\"running\",\"backend\":\"C++17\","
        "\"timezone\":\"IST\","
        "\"serverWallMs\":" + to_string(currentWallMs()) + "}");
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
    j << "{\"success\":true,\"serverWallMs\":" << currentWallMs() << ",\"centres\":[";
    for (size_t i = 0; i < st.centres.size(); ++i) {
        const auto& c = st.centres[i];
        if (i) j << ",";

        string status = centreStatusString(c);
        int elapsedSec = centreElapsedSeconds(c);
        int remSec = centreRemainingSeconds(c);

        j << "{\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"testId\":\"" << escapeJson(c.testId) << "\","
          << "\"duration\":" << c.durationMinutes << ","
          << "\"durationMinutes\":" << c.durationMinutes << ","
          << "\"available\":" << (c.available() ? "true" : "false") << ","
          << "\"status\":\"" << escapeJson(status) << "\","
          << "\"currentPatient\":"
          << (c.currentPatient.empty() ? "null" : "\"" + escapeJson(c.currentPatient) + "\"")
          << ",\"currentTestId\":"
          << (c.currentTestId.empty() ? "null" : "\"" + escapeJson(c.currentTestId) + "\"")
          << ",\"elapsedSeconds\":" << elapsedSec
          << ",\"remainingSeconds\":" << remSec
          << ",\"completedCount\":" << c.completedCount
          << ",\"busyMinutes\":" << c.busyMinutes
          << "}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleListPatients(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,"
      << "\"nextPatientId\":\"" << escapeJson(st.peekNextPatientId()) << "\","
      << "\"patients\":[";
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

    JsonValue jroot;
    {
        JsonParser parser(body);
        if (!parser.parse(jroot) || !jroot.isObj()) {
            return errorResponse(400, "Bad Request", "Invalid JSON body");
        }
    }

    string name = jroot.has("name") ? jroot.at("name").asStr("") : "";
    bool emergency = jroot.has("emergency") &&
                     jroot.at("emergency").isBool() &&
                     jroot.at("emergency").bval;

    vector<string> tests;
    if (jroot.has("tests") && jroot.at("tests").isArr()) {
        for (const auto& v : jroot.at("tests").arr) {
            if (v.isStr()) tests.push_back(v.sval);
        }
    }

    if (tests.empty()) return errorResponse(400, "Bad Request", "At least one test is required");
    for (auto& t : tests) if (!st.testExists(t))
        return errorResponse(400, "Bad Request", "Unknown test ID: " + t);

    string id = st.generatePatientId();

    if (!st.addPatient(id, name, tests, emergency)) {
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
    j << "]},"
      << "\"nextPatientId\":\"" << escapeJson(st.peekNextPatientId()) << "\"}";
    return jsonResponse(201, "Created", j.str());
}

string handleDeletePatient(ServerState& st, const string& id) {
    if (id.empty()) return errorResponse(400, "Bad Request", "Patient ID is required");
    if (!st.removePatient(id)) return errorResponse(404, "Not Found", "Patient not found");
    return jsonResponse(200, "OK",
        "{\"success\":true,\"removed\":\"" + escapeJson(id) + "\","
        "\"nextPatientId\":\"" + escapeJson(st.peekNextPatientId()) + "\"}");
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
              << "\"testName\":\"" << escapeJson(st.testName(kv.second[i].testId)) << "\","
              << "\"centreId\":\"" << escapeJson(kv.second[i].centreId) << "\","
              << "\"centreName\":\""
              << escapeJson(st.findCentre(kv.second[i].centreId) ?
                              st.findCentre(kv.second[i].centreId)->name : "") << "\","
              << "\"startTime\":" << kv.second[i].startTime << ","
              << "\"duration\":" << kv.second[i].duration << ","
              << "\"endTime\":" << (kv.second[i].startTime + kv.second[i].duration) << "}";
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
          << ",\"centreName\":\""
          << escapeJson(st.findCentre(r.events[i].centreId) ?
                          st.findCentre(r.events[i].centreId)->name : "") << "\""
          << ",\"patientId\":\"" << escapeJson(r.events[i].patientId) << "\""
          << ",\"testId\":\"" << escapeJson(r.events[i].testId) << "\""
          << ",\"testName\":\"" << escapeJson(st.testName(r.events[i].testId)) << "\"}";
    }
    j << "],";

    j << "\"metrics\":{"
      << "\"makespan\":" << r.metrics.makespan << ","
      << "\"sumCompletionTimes\":" << r.metrics.sumCompletionTimes << ","
      << "\"sumWaitingTimes\":" << r.metrics.sumWaitingTimes << ","
      << "\"sumMovementTimes\":" << r.metrics.sumMovementTimes << ","
      << "\"testsCompleted\":" << r.metrics.testsCompleted << ","
      << "\"patientsCompleted\":" << r.metrics.patientsCompleted << "}";

    j << "}";
    return jsonResponse(200, "OK", j.str());
}

string handleResetQueue(ServerState& st) {
    st.reset();
    return jsonResponse(200, "OK",
        "{\"success\":true,\"message\":\"Queue cleared\","
        "\"nextPatientId\":\"" + escapeJson(st.peekNextPatientId()) + "\"}");
}

string handleGetEvents(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,"
      << "\"serverWallMs\":" << currentWallMs() << ","
      << "\"total\":" << st.eventLog.size() << ","
      << "\"events\":[";

    for (size_t i = 0; i < st.eventLog.size(); ++i) {
        const auto& e = st.eventLog[i];
        if (i) j << ",";
        j << "{\"id\":" << e.id
          << ",\"wallMs\":" << e.wallMs
          << ",\"type\":\"" << escapeJson(e.type) << "\""
          << ",\"patientId\":\"" << escapeJson(e.patientId) << "\""
          << ",\"patientName\":\"" << escapeJson(e.patientName) << "\""
          << ",\"testId\":\"" << escapeJson(e.testId) << "\""
          << ",\"testName\":\"" << escapeJson(e.testName) << "\""
          << ",\"centreId\":\"" << escapeJson(e.centreId) << "\""
          << ",\"centreName\":\"" << escapeJson(e.centreName) << "\""
          << ",\"durationMinutes\":" << e.durationMinutes
          << ",\"actualElapsedSeconds\":" << e.actualElapsedSeconds
          << "}";
    }

    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleNextPatient(ServerState& st, const string& centreId) {
    TestingCentre* centre = st.findCentre(centreId);
    if (!centre) return errorResponse(404, "Not Found", "Centre not found");
    if (!centre->available()) {
        string status = centreStatusString(*centre);
        if (status == "AWAITING_CONFIRMATION") {
            return errorResponse(409, "Conflict",
                "Centre is awaiting completion confirmation for patient " + centre->currentPatient);
        }
        return errorResponse(409, "Conflict", "Centre is currently busy");
    }

    long maxSeq = st.nextSeq > 0 ? st.nextSeq - 1 : 0;

    vector<SimPatient> sim;
    for (auto& p : st.patients) {
        if (p.status != PatientStatus::WAITING) continue;
        if (!p.currentCentre.empty()) continue;
        SimPatient sp;
        sp.id = p.id;
        sp.addedSeq = p.addedSeq;
        sp.emergency = p.emergency;
        sp.readyAt = -(maxSeq - p.addedSeq);

        for (auto& ts : p.testStates) {
            if (ts.status == TestStatus::WAITING) sp.remaining.push_back(ts.testId);
        }
        if (!sp.remaining.empty()) sim.push_back(sp);
    }

    vector<SimCentre> simC;
    long long nowMs = currentSteadyMs();
    for (auto& c : st.centres) {
        // [v1.7.2 FIX] Only build simulation for the requested centre.
        // Without this, scheduleStep() could match an eligible patient
        // to a DIFFERENT centre, consume that patient from the pool,
        // and leave this centre with no assignment — causing
        // patient:null in the /next response even when a valid
        // candidate existed.
        if (c.id != centreId) continue;

        SimCentre sc;
        sc.id = c.id;
        sc.name = c.name;
        sc.testId = c.testId;
        sc.duration = c.durationMinutes;
        if (!c.currentPatient.empty()) {
            sc.currentPatient = c.currentPatient;
            if (c.expectedCompletionMs > 0) {
                if (nowMs >= c.expectedCompletionMs) {
                    sc.busyUntil = 0;
                } else {
                    long long remMs = c.expectedCompletionMs - nowMs;
                    long remMin = (long)((remMs + 59999) / 60000);
                    if (remMin < 1) remMin = 1;
                    sc.busyUntil = remMin;
                }
            } else {
                sc.busyUntil = (c.remainingMin > 0
                                ? (long)c.remainingMin
                                : (long)c.durationMinutes);
            }
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

    long long startMs = currentSteadyMs();
    centre->startedAtMs = startMs;
    centre->expectedCompletionMs = startMs + (long long)centre->durationMinutes * 60LL * 1000LL;

    p->currentCentre = centreId;
    p->currentTestId = chosen->testId;
    centre->currentPatient = p->id;
    centre->currentTestId = chosen->testId;
    centre->remainingMin = centre->durationMinutes;

    st.logEvent(
        "start",
        p->id, p->name,
        chosen->testId, st.testName(chosen->testId),
        centre->id, centre->name,
        centre->durationMinutes,
        0
    );

    ostringstream j;
    j << "{\"success\":true,\"centreId\":\"" << escapeJson(centreId) << "\","
      << "\"patient\":{"
      << "\"id\":\"" << escapeJson(p->id) << "\","
      << "\"name\":\"" << escapeJson(p->name) << "\","
      << "\"testId\":\"" << escapeJson(chosen->testId) << "\","
      << "\"testName\":\"" << escapeJson(st.testName(chosen->testId)) << "\""
      << "},"
      << "\"startedAtMs\":" << startMs << ","
      << "\"expectedCompletionMs\":" << centre->expectedCompletionMs << ","
      << "\"assignmentStatus\":\"ASSIGNED\"}";
    return jsonResponse(200, "OK", j.str());
}

string handleCompleteTest(ServerState& st, const string& body) {
    if (body.empty()) return errorResponse(400, "Bad Request", "Request body is empty");

    JsonValue root;
    JsonParser parser(body);
    if (!parser.parse(root) || !root.isObj()) {
        return errorResponse(400, "Bad Request", "Invalid JSON body");
    }

    const string patientId = root.has("patientId") ? root.at("patientId").asStr("") : "";
    const string testId = root.has("testId") ? root.at("testId").asStr("") : "";
    string centreId = root.has("centreId") ? root.at("centreId").asStr("") : "";

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

    if (p->currentCentre.empty()) {
        return errorResponse(409, "Conflict",
            "Patient is not assigned to an active testing centre");
    }
    if (p->currentTestId != testId) {
        return errorResponse(409, "Conflict",
            "Requested test does not match the patient's active test");
    }
    if (!centreId.empty() && centreId != p->currentCentre) {
        return errorResponse(409, "Conflict",
            "Patient is assigned to a different centre");
    }
    centreId = p->currentCentre;

    TestingCentre* assignedCentre = st.findCentre(centreId);
    if (!assignedCentre) {
        return errorResponse(404, "Not Found", "Assigned testing centre not found");
    }
    if (assignedCentre->currentPatient != patientId) {
        return errorResponse(409, "Conflict",
            "Centre is not serving this patient");
    }
    if (assignedCentre->currentTestId != testId ||
        assignedCentre->testId != testId) {
        return errorResponse(409, "Conflict",
            "Test does not match the centre assignment");
    }

    long long actualElapsedSec = 0;
    if (assignedCentre->startedAtMs > 0) {
        long long deltaMs = currentSteadyMs() - assignedCentre->startedAtMs;
        if (deltaMs > 0) actualElapsedSec = deltaMs / 1000;
    }

    ts->status = TestStatus::COMPLETED;

    assignedCentre->currentPatient = "";
    assignedCentre->currentTestId = "";
    assignedCentre->remainingMin = 0;
    assignedCentre->startedAtMs = 0;
    assignedCentre->expectedCompletionMs = 0;
    assignedCentre->completedCount++;
    assignedCentre->busyMinutes += assignedCentre->durationMinutes;

    p->currentCentre = "";
    p->currentTestId = "";

    bool allDone = true;
    for (auto& t : p->testStates) if (t.status != TestStatus::COMPLETED) { allDone = false; break; }
    if (allDone) p->status = PatientStatus::COMPLETED;

    st.logEvent(
        "complete",
        patientId, p->name,
        testId, st.testName(testId),
        assignedCentre->id, assignedCentre->name,
        assignedCentre->durationMinutes,
        actualElapsedSec
    );

    ostringstream j;
    j << "{\"success\":true,\"patientId\":\"" << escapeJson(patientId) << "\","
      << "\"testId\":\"" << escapeJson(testId) << "\","
      << "\"completed\":true,\"centreId\":\"" << escapeJson(centreId) << "\","
      << "\"actualElapsedSeconds\":" << actualElapsedSec << "}";
    return jsonResponse(200, "OK", j.str());
}

string handleGetConfiguration(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,"
      << "\"hospitalName\":\"" << escapeJson(Config::APP_NAME) << "\","
      << "\"tests\":[";
    for (size_t i = 0; i < st.tests.size(); ++i) {
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(st.tests[i].id) << "\","
          << "\"name\":\"" << escapeJson(st.tests[i].name) << "\","
          << "\"durationMinutes\":" << st.tests[i].durationMinutes << "}";
    }
    j << "],\"centres\":[";
    for (size_t i = 0; i < st.centres.size(); ++i) {
        const auto& c = st.centres[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"testId\":\"" << escapeJson(c.testId) << "\","
          << "\"machineCount\":1,"
          << "\"durationMinutes\":" << c.durationMinutes << ","
          << "\"enabled\":true}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

string handleImportConfiguration(ServerState& st, const string& body) {
    if (body.empty())
        return errorResponse(400, "Bad Request", "Request body is empty");

    if (!st.patients.empty()) {
        return errorResponse(409, "Conflict",
            "Cannot import configuration while patients exist. Clear the queue first.");
    }
    for (const auto& c : st.centres) {
        if (!c.currentPatient.empty()) {
            return errorResponse(409, "Conflict",
                "Cannot import configuration while a centre is busy.");
        }
    }

    JsonValue root;
    {
        JsonParser parser(body);
        if (!parser.parse(root) || !root.isObj()) {
            return errorResponse(400, "Bad Request", "Invalid JSON configuration");
        }
    }

    if (!root.has("tests") || !root.at("tests").isArr() ||
        root.at("tests").arr.empty()) {
        return errorResponse(400, "Bad Request", "Configuration must include a non-empty 'tests' array");
    }
    if (!root.has("centres") || !root.at("centres").isArr() ||
        root.at("centres").arr.empty()) {
        return errorResponse(400, "Bad Request", "Configuration must include a non-empty 'centres' array");
    }

    vector<DiagnosticTest> newTests;
    set<string> testIds;

    for (const auto& t : root.at("tests").arr) {
        if (!t.isObj())
            return errorResponse(400, "Bad Request", "Each test must be an object");

        string id = t.has("id") ? t.at("id").asStr("") : "";
        string name = t.has("name") ? t.at("name").asStr("") : "";
        int dur = 0;
        if (t.has("durationMinutes") && t.at("durationMinutes").isNum()) {
            dur = (int)t.at("durationMinutes").nval;
        }

        if (id.empty())
            return errorResponse(400, "Bad Request", "Test 'id' is required");
        if (name.empty())
            return errorResponse(400, "Bad Request", "Test 'name' is required (id=" + id + ")");
        if (dur <= 0)
            return errorResponse(400, "Bad Request",
                "Test duration must be positive (id=" + id + ")");
        if (testIds.count(id))
            return errorResponse(400, "Bad Request", "Duplicate test id: " + id);

        testIds.insert(id);
        newTests.push_back({id, name, dur});
    }

    vector<TestingCentre> newCentres;
    set<string> centreIds;

    for (const auto& c : root.at("centres").arr) {
        if (!c.isObj())
            return errorResponse(400, "Bad Request", "Each centre must be an object");

        if (c.has("enabled") && c.at("enabled").isBool() &&
            !c.at("enabled").bval) {
            continue;
        }

        string id = c.has("id") ? c.at("id").asStr("") : "";
        string name = c.has("name") ? c.at("name").asStr("") : "";
        string testId = c.has("testId") ? c.at("testId").asStr("") : "";
        int dur = 0;
        if (c.has("durationMinutes") && c.at("durationMinutes").isNum()) {
            dur = (int)c.at("durationMinutes").nval;
        }

        if (id.empty())
            return errorResponse(400, "Bad Request", "Centre 'id' is required");
        if (name.empty())
            return errorResponse(400, "Bad Request", "Centre 'name' is required (id=" + id + ")");
        if (testId.empty())
            return errorResponse(400, "Bad Request", "Centre 'testId' is required (id=" + id + ")");
        if (!testIds.count(testId))
            return errorResponse(400, "Bad Request",
                "Centre '" + id + "' references unknown test '" + testId + "'");
        if (dur <= 0)
            return errorResponse(400, "Bad Request",
                "Centre duration must be positive (id=" + id + ")");
        if (centreIds.count(id))
            return errorResponse(400, "Bad Request", "Duplicate centre id: " + id);

        if (c.has("machineCount") && c.at("machineCount").isNum()) {
            int mc = (int)c.at("machineCount").nval;
            if (mc != 1) {
                return errorResponse(400, "Bad Request",
                    "Centre '" + id + "' has machineCount=" + to_string(mc) +
                    ". Only machineCount=1 is supported.");
            }
        }

        centreIds.insert(id);
        TestingCentre nc;
        nc.id = id;
        nc.name = name;
        nc.testId = testId;
        nc.durationMinutes = dur;
        newCentres.push_back(nc);
    }

    if (newCentres.empty())
        return errorResponse(400, "Bad Request",
            "Configuration must contain at least one enabled centre");

    for (auto& nc : newCentres) {
        const TestingCentre* old = st.findCentre(nc.id);
        if (old) {
            nc.busyMinutes = old->busyMinutes;
            nc.completedCount = old->completedCount;
        }
    }

    st.tests = std::move(newTests);
    st.centres = std::move(newCentres);

    return handleGetConfiguration(st);
}

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
        if (p == "/")                    return handleHome();
        if (p == "/api/health")          return handleHealth();
        if (p == "/api/tests")           return handleTests(st);
        if (p == "/api/centres")         return handleCentres(st);
        if (p == "/api/patients")        return handleListPatients(st);
        if (p == "/api/queue")           return handleGetQueue(st);
        if (p == "/api/events")          return handleGetEvents(st);
        if (p == "/api/configuration")   return handleGetConfiguration(st);

        string rest;
        if (matchPrefix(p, "/api/centres/", rest) &&
            rest.size() > 5 &&
            rest.compare(rest.size() - 5, 5, "/next") == 0) {
            return handleNextPatient(st, rest.substr(0, rest.size() - 5));
        }
    }

    if (m == "POST") {
        if (p == "/api/patients")        return handleAddPatient(st, req.body);
        if (p == "/api/queue/reset")     return handleResetQueue(st);
        if (p == "/api/tests/complete")  return handleCompleteTest(st, req.body);
        if (p == "/api/configuration")   return handleImportConfiguration(st, req.body);
    }

    if (m == "DELETE") {
        string rest;
        if (matchPrefix(p, "/api/patients/", rest))
            return handleDeletePatient(st, rest);
    }

    bool known =
        p == "/" || p == "/api/health" || p == "/api/tests" ||
        p == "/api/centres" || p == "/api/patients" ||
        p == "/api/queue" || p == "/api/events" || p == "/api/tests/complete" ||
        p == "/api/configuration" ||
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
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(Config::getPort());
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
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
    cout << "\n========================================\n"
         << "       PRATIKSHA HOSPITAL SYSTEM\n"
         << "              v" << Config::VERSION << "\n"
         << "========================================\n";

    ServerState state;
    int srv = createServerSocket();
    if (srv < 0) { cerr << "[FATAL] Server could not start." << endl; return 1; }

    cout << "[SUCCESS] C++ server started." << endl
         << "[SERVER] http://127.0.0.1:" << Config::getPort() << endl
         << "[API] GET    /api/health" << endl
         << "[API] GET    /api/tests" << endl
         << "[API] GET    /api/centres" << endl
         << "[API] GET    /api/centres/{id}/next" << endl
         << "[API] GET    /api/patients" << endl
         << "[API] POST   /api/patients           (auto ID)" << endl
         << "[API] DELETE /api/patients/{id}" << endl
         << "[API] POST   /api/tests/complete" << endl
         << "[API] GET    /api/queue" << endl
         << "[API] POST   /api/queue/reset" << endl
         << "[API] GET    /api/events             (event history)" << endl
         << "[API] GET    /api/configuration" << endl
         << "[API] POST   /api/configuration      (import JSON)" << endl
         << "[INFO] Press Control+C to stop." << endl
         << "========================================\n";

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