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
// Backend v1.9.0
//
// Changes over v1.8.0 (per architecture guidance):
//   [Change 1] DiagnosticTest model now carries bufferMinutes
//              and `enabled`. Test definitions are fully
//              configurable; no test is treated as special.
//   [Change 2] TestingCentre model expanded with bufferMinutes,
//              machineCount, activeMachines, maintenanceMachines,
//              currentActiveTests, maxSimultaneousTests and
//              `enabled`. Existing positional initialisers still
//              work because the new fields have defaults.
//   [Change 3] handleGetConfiguration() now returns the ACTUAL
//              stored state — no more hard-coded machineCount:1
//              or enabled:true. Hospital name / ID come from
//              ServerState (NOT Config::APP_NAME, which is the
//              application name, not a hospital name).
//   [Change 4] handleImportConfiguration() accepts and stores
//              all the new fields, and the old
//              "machineCount must equal 1" restriction is gone.
//              Validation is more thorough (duplicate IDs,
//              unknown testIds, non-positive durations, machine
//              count consistency). This endpoint still mutates
//              in-memory demo state so legacy GET /api/queue,
//              GET /api/centres, etc. keep working; the new
//              architecture uses the stateless endpoint below.
//   [Change 5] Multi-machine scheduling lives in
//              calculateStatelessQueue() (already present in
//              v1.8.0). The legacy single-slot in-memory
//              computeQueue() is retained for the demo mode
//              and clearly documented as single-slot.
//   [Change 6/7] Stateless /api/queue/calculate continues to
//              accept per-request hospital snapshots so that
//              nothing is persisted for the new architecture.
//
// All v1.8.0 features preserved:
//   POST /api/queue/calculate   (stateless, capacity-aware)
//   handleNextPatient()         auto-mode single-centre fix
//   PORT env override
//   Live centre timing + AWAITING_CONFIRMATION
//   Event history, JSON config import, all legacy endpoints
// ============================================================

namespace Config {
    inline int getPort() {
        const char* envPort = std::getenv("PORT");

        if (envPort != nullptr) {
            int port = std::atoi(envPort);

            if (port > 0 && port <= 65535) {
                return port;
            }
        }

        return 8081;
    }

    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 131072; // 128KB
    constexpr int QUEUE_MAX_TIME = 5000;
    constexpr long INF_TIME = 1000000000L;
    constexpr double LAMBDA = 0.5;
    constexpr int MOVE_TIME_MINUTES = 3;
    constexpr size_t MAX_EVENT_LOG = 500;

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.9.0";

    // [Change 3] Default hospital name for the demo mode.
    // The frontend holds the real hospital name in localStorage;
    // this is only used as a fallback for the legacy endpoints.
    const string DEFAULT_HOSPITAL_ID   = "H-DEFAULT";
    const string DEFAULT_HOSPITAL_NAME = "Default Hospital";
}

// ------------------------------------------------------------
// Models
// ------------------------------------------------------------

enum class TestStatus { WAITING, COMPLETED };
enum class PatientStatus { WAITING, COMPLETED };

// [Change 1] DiagnosticTest now carries bufferMinutes and
// enabled so that tests are fully configurable and match the
// frontend hospital.tests schema.
struct DiagnosticTest {
    string id;
    string name;
    int durationMinutes = 0;
    int bufferMinutes = 0;
    bool enabled = true;
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

// [Change 2] TestingCentre expanded. The first block of fields
// matches the previous layout, so all existing positional
// initialisers (e.g. {"A1", "X-Ray Room", "A", 10}) keep
// working — the new fields simply use their defaults.
//
// Note about multi-machine support:
//   The legacy in-memory computeQueue() / handleNextPatient()
//   still model a centre as a single serving slot. Parallel
//   scheduling is implemented in the stateless
//   calculateStatelessQueue() path, which the frontend uses.
//   machineCount / activeMachines / maintenanceMachines are
//   stored and validated here so the /api/queue/calculate
//   request can be built with accurate values.
struct TestingCentre {
    // Existing layout — DO NOT reorder these; they are set by
    // positional aggregate init in ServerState().
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

    // [Change 2] New configuration fields.
    int bufferMinutes          = 0;
    int machineCount           = 1;
    int activeMachines         = 1;
    int maintenanceMachines    = 0;
    int currentActiveTests     = 0;
    int maxSimultaneousTests   = 1;
    bool enabled               = true;

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
// Stateless calc models (POST /api/queue/calculate)
// Unchanged from v1.8.0 — already fully dynamic.
// ------------------------------------------------------------

struct StatelessTest {
    string id;
    string name;
    int durationMinutes = 0;
    int bufferMinutes = 0;
    bool enabled = true;
};

struct StatelessCentre {
    string id;
    string name;
    vector<string> testIds;
    int totalMachines = 1;
    int activeMachines = 1;
    int maintenanceMachines = 0;
    int currentActiveTests = 0;
    int maxSimultaneousTests = 1;
    int durationMinutes = 0;
    int bufferMinutes = 0;
    bool enabled = true;
};

struct StatelessPatientTest {
    string testId;
    string status;
};

struct StatelessPatient {
    string id;
    string name;
    vector<StatelessPatientTest> tests;
};

struct StatelessCalcRequest {
    vector<StatelessTest> tests;
    vector<StatelessCentre> centres;
    vector<StatelessPatient> patients;
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

int centreRemainingSeconds(const TestingCentre& c) {
    if (c.currentPatient.empty()) return 0;
    if (c.expectedCompletionMs <= 0) return 0;
    long long now = currentSteadyMs();
    if (now >= c.expectedCompletionMs) return 0;
    return (int)((c.expectedCompletionMs - now) / 1000);
}

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
// Server State (in-memory backend model, kept for /api/queue)
// ------------------------------------------------------------

class ServerState {
public:
    vector<DiagnosticTest> tests;
    vector<TestingCentre> centres;
    vector<Patient> patients;
    long nextSeq = 0;

    int nextPatientNumber = 1;

    // [Change 3] Hospital identity for legacy /api/configuration.
    // Real hospital name lives in the frontend localStorage;
    // these are only demo defaults.
    string hospitalId   = Config::DEFAULT_HOSPITAL_ID;
    string hospitalName = Config::DEFAULT_HOSPITAL_NAME;

    vector<EventLogEntry> eventLog;
    int nextEventId = 1;

    ServerState() {
        // [Change 1] Test defaults now use the expanded struct.
        tests = {
            {"A", "X-Ray",        10, 5, true},
            {"B", "Blood Test",    5, 3, true},
            {"C", "Ultrasound",   20, 5, true},
            {"D", "CT Scan",      30, 8, true}
        };
        // [Change 2] Centre defaults use the expanded struct.
        // Only the first four fields are positional; the rest
        // pick up defaults (machineCount=1, activeMachines=1,
        // maxSimultaneousTests=1, enabled=true, etc.).
        TestingCentre a1;
        a1.id = "A1"; a1.name = "X-Ray Room";       a1.testId = "A"; a1.durationMinutes = 10;
        a1.bufferMinutes = 5;  a1.machineCount = 1; a1.activeMachines = 1;
        a1.maxSimultaneousTests = 1;

        TestingCentre b1;
        b1.id = "B1"; b1.name = "Blood Test Lab";   b1.testId = "B"; b1.durationMinutes = 5;
        b1.bufferMinutes = 3;  b1.machineCount = 1; b1.activeMachines = 1;
        b1.maxSimultaneousTests = 1;

        TestingCentre c1;
        c1.id = "C1"; c1.name = "Ultrasound Room";  c1.testId = "C"; c1.durationMinutes = 20;
        c1.bufferMinutes = 5;  c1.machineCount = 1; c1.activeMachines = 1;
        c1.maxSimultaneousTests = 1;

        TestingCentre d1;
        d1.id = "D1"; d1.name = "CT Scan Room";     d1.testId = "D"; d1.durationMinutes = 30;
        d1.bufferMinutes = 8;  d1.machineCount = 1; d1.activeMachines = 1;
        d1.maxSimultaneousTests = 1;

        centres = { a1, b1, c1, d1 };
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
// JSON escaping
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
// Minimal JSON parser (unchanged)
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
// In-memory simulation (legacy demo mode, single-slot per centre)
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

// ============================================================
// Stateless capacity-aware queue calculation
// (unchanged from v1.8.0 — already dynamic + multi-slot)
// ============================================================

QueueResult calculateStatelessQueue(const StatelessCalcRequest& req) {
    QueueResult result;

    map<string, vector<long>> centreSlots;

    for (const auto& c : req.centres) {
        if (!c.enabled) continue;

        int slotCount = c.activeMachines;
        if (slotCount <= 0) slotCount = c.maxSimultaneousTests;
        if (slotCount <= 0) slotCount = 1;

        if (c.maxSimultaneousTests > 0 &&
            c.maxSimultaneousTests < slotCount) {
            slotCount = c.maxSimultaneousTests;
        }

        vector<long> slots((size_t)slotCount, 0L);

        int busySlots = c.currentActiveTests;
        if (busySlots < 0) busySlots = 0;
        if (busySlots > slotCount) busySlots = slotCount;
        long initialOccupancy = (long)(c.durationMinutes > 0
                                       ? c.durationMinutes : 0);
        for (int i = 0; i < busySlots; ++i) {
            slots[(size_t)i] = initialOccupancy;
        }

        centreSlots[c.id] = std::move(slots);
    }

    map<string, const StatelessTest*> testLookup;
    for (const auto& t : req.tests) {
        if (!t.id.empty()) testLookup[t.id] = &t;
    }

    long maxTime = 0;
    long sumWaiting = 0;
    long sumCompletion = 0;
    int testsCompleted = 0;
    int patientsCompleted = 0;

    for (const auto& p : req.patients) {
        result.perPatient[p.id] = {};
        long patientAvail = 0;

        for (const auto& ts : p.tests) {
            if (ts.status == "COMPLETED") continue;

            StatelessCentre* chosenCentre = nullptr;
            for (auto& c : req.centres) {
                if (!c.enabled) continue;
                bool handles = false;
                for (const auto& tid : c.testIds) {
                    if (tid == ts.testId) { handles = true; break; }
                }
                if (!handles) continue;
                if (!centreSlots.count(c.id)) continue;
                chosenCentre = const_cast<StatelessCentre*>(&c);
                break;
            }
            if (!chosenCentre) continue;

            auto slotIt = centreSlots.find(chosenCentre->id);
            if (slotIt == centreSlots.end() || slotIt->second.empty()) continue;
            vector<long>& slots = slotIt->second;

            long earliestSlot = slots[0];
            size_t earliestIdx = 0;
            for (size_t s = 1; s < slots.size(); ++s) {
                if (slots[s] < earliestSlot) {
                    earliestSlot = slots[s];
                    earliestIdx = s;
                }
            }

            long startTime = std::max(earliestSlot, patientAvail);

            int dur = chosenCentre->durationMinutes;
            if (dur <= 0) {
                auto tIt = testLookup.find(ts.testId);
                if (tIt != testLookup.end() && tIt->second->durationMinutes > 0) {
                    dur = tIt->second->durationMinutes;
                }
            }
            if (dur <= 0) dur = 10;

            int buf = chosenCentre->bufferMinutes;
            if (buf < 0) buf = 0;

            long endTime = startTime + dur;

            if (startTime > patientAvail) {
                sumWaiting += (startTime - patientAvail);
            }

            slots[earliestIdx] = endTime + buf;
            patientAvail = endTime;

            QueueEvent startEv;
            startEv.time = (int)startTime;
            startEv.type = "start";
            startEv.centreId = chosenCentre->id;
            startEv.patientId = p.id;
            startEv.testId = ts.testId;
            result.events.push_back(startEv);

            QueueEvent completeEv;
            completeEv.time = (int)endTime;
            completeEv.type = "complete";
            completeEv.centreId = chosenCentre->id;
            completeEv.patientId = p.id;
            completeEv.testId = ts.testId;
            result.events.push_back(completeEv);

            QueueStep step;
            step.testId = ts.testId;
            step.centreId = chosenCentre->id;
            step.startTime = (int)startTime;
            step.duration = (int)dur;
            result.perPatient[p.id].push_back(step);

            testsCompleted++;
            if (endTime > maxTime) maxTime = endTime;
        }

        if (!result.perPatient[p.id].empty()) {
            sumCompletion += (result.perPatient[p.id].back().startTime +
                              result.perPatient[p.id].back().duration);
            patientsCompleted++;
        }
    }

    std::sort(result.events.begin(), result.events.end(),
        [](const QueueEvent& a, const QueueEvent& b) {
            if (a.time != b.time) return a.time < b.time;
            return a.type == "start" && b.type != "start";
        });

    result.totalTime = (int)maxTime;
    result.metrics.makespan = (int)maxTime;
    result.metrics.sumWaitingTimes = sumWaiting;
    result.metrics.sumCompletionTimes = sumCompletion;
    result.metrics.sumMovementTimes = 0;
    result.metrics.testsCompleted = testsCompleted;
    result.metrics.patientsCompleted = patientsCompleted;

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

// [Change 1] Return bufferMinutes + enabled as well.
string handleTests(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,\"tests\":[";
    for (size_t i = 0; i < st.tests.size(); ++i) {
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(st.tests[i].id) << "\","
          << "\"name\":\"" << escapeJson(st.tests[i].name) << "\","
          << "\"durationMinutes\":" << st.tests[i].durationMinutes << ","
          << "\"bufferMinutes\":" << st.tests[i].bufferMinutes << ","
          << "\"enabled\":" << (st.tests[i].enabled ? "true" : "false") << "}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

// [Change 2] Return all expanded centre fields.
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
          << "\"bufferMinutes\":" << c.bufferMinutes << ","
          << "\"machineCount\":" << c.machineCount << ","
          << "\"activeMachines\":" << c.activeMachines << ","
          << "\"maintenanceMachines\":" << c.maintenanceMachines << ","
          << "\"currentActiveTests\":" << c.currentActiveTests << ","
          << "\"maxSimultaneousTests\":" << c.maxSimultaneousTests << ","
          << "\"enabled\":" << (c.enabled ? "true" : "false") << ","
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

// ============================================================
// POST /api/queue/calculate  (stateless, primary new path)
// Unchanged from v1.8.0 — fully dynamic.
// ============================================================

string handleStatelessQueueCalculate(const string& body) {
    if (body.empty()) {
        return errorResponse(400, "Bad Request", "Request body is empty");
    }

    JsonValue root;
    {
        JsonParser parser(body);
        if (!parser.parse(root) || !root.isObj()) {
            return errorResponse(400, "Bad Request", "Invalid JSON body");
        }
    }

    if (!root.has("hospital") || !root.at("hospital").isObj()) {
        return errorResponse(400, "Bad Request",
            "Missing 'hospital' object");
    }

    const JsonValue& hospital = root.at("hospital");

    StatelessCalcRequest req;

    if (hospital.has("tests") && hospital.at("tests").isArr()) {
        for (const auto& t : hospital.at("tests").arr) {
            if (!t.isObj()) continue;
            StatelessTest st;
            st.id = t.has("id") ? t.at("id").asStr("") : "";
            st.name = t.has("name") ? t.at("name").asStr(st.id) : st.id;
            if (t.has("durationMinutes") && t.at("durationMinutes").isNum()) {
                st.durationMinutes = (int)t.at("durationMinutes").nval;
            }
            if (t.has("bufferMinutes") && t.at("bufferMinutes").isNum()) {
                st.bufferMinutes = (int)t.at("bufferMinutes").nval;
            }
            if (t.has("enabled") && t.at("enabled").isBool()) {
                st.enabled = t.at("enabled").bval;
            }
            if (st.durationMinutes <= 0) {
                return errorResponse(400, "Bad Request",
                    "Test '" + st.id + "' has invalid durationMinutes");
            }
            if (!st.id.empty()) req.tests.push_back(st);
        }
    }

    if (hospital.has("centres") && hospital.at("centres").isArr()) {
        for (const auto& c : hospital.at("centres").arr) {
            if (!c.isObj()) continue;
            StatelessCentre sc;
            sc.id = c.has("id") ? c.at("id").asStr("") : "";
            sc.name = c.has("name") ? c.at("name").asStr(sc.id) : sc.id;

            if (c.has("testIds") && c.at("testIds").isArr()) {
                for (const auto& tid : c.at("testIds").arr) {
                    if (tid.isStr()) sc.testIds.push_back(tid.sval);
                }
            } else if (c.has("testId") && c.at("testId").isStr()) {
                sc.testIds.push_back(c.at("testId").sval);
            }

            if (c.has("totalMachines") && c.at("totalMachines").isNum())
                sc.totalMachines = (int)c.at("totalMachines").nval;
            if (c.has("activeMachines") && c.at("activeMachines").isNum())
                sc.activeMachines = (int)c.at("activeMachines").nval;
            if (c.has("maintenanceMachines") && c.at("maintenanceMachines").isNum())
                sc.maintenanceMachines = (int)c.at("maintenanceMachines").nval;
            if (c.has("currentActiveTests") && c.at("currentActiveTests").isNum())
                sc.currentActiveTests = (int)c.at("currentActiveTests").nval;
            if (c.has("maxSimultaneousTests") && c.at("maxSimultaneousTests").isNum())
                sc.maxSimultaneousTests = (int)c.at("maxSimultaneousTests").nval;
            if (c.has("durationMinutes") && c.at("durationMinutes").isNum())
                sc.durationMinutes = (int)c.at("durationMinutes").nval;
            if (c.has("bufferMinutes") && c.at("bufferMinutes").isNum())
                sc.bufferMinutes = (int)c.at("bufferMinutes").nval;

            sc.enabled = true;
            if (c.has("enabled") && c.at("enabled").isBool() && !c.at("enabled").bval)
                sc.enabled = false;
            if (c.has("status") && c.at("status").isStr()) {
                const string& stt = c.at("status").sval;
                if (stt == "INACTIVE" || stt == "Inactive" || stt == "inactive")
                    sc.enabled = false;
            }

            if (!sc.id.empty()) req.centres.push_back(sc);
        }
    }

    if (hospital.has("patients") && hospital.at("patients").isArr()) {
        for (const auto& p : hospital.at("patients").arr) {
            if (!p.isObj()) continue;
            StatelessPatient sp;
            sp.id = p.has("id") ? p.at("id").asStr("") : "";
            sp.name = p.has("name") ? p.at("name").asStr("") : "";

            if (p.has("tests") && p.at("tests").isArr()) {
                for (const auto& t : p.at("tests").arr) {
                    if (!t.isObj()) continue;
                    StatelessPatientTest spt;
                    spt.testId = t.has("testId") ? t.at("testId").asStr("") : "";
                    spt.status = t.has("status") ? t.at("status").asStr("WAITING")
                                                 : "WAITING";
                    if (!spt.testId.empty()) sp.tests.push_back(spt);
                }
            }

            if (!sp.id.empty()) req.patients.push_back(sp);
        }
    }

    if (req.tests.empty()) {
        return errorResponse(400, "Bad Request",
            "Configuration must include at least one test");
    }
    if (req.centres.empty()) {
        return errorResponse(400, "Bad Request",
            "Configuration must include at least one centre");
    }

    set<string> knownTestIds;
    for (const auto& t : req.tests) knownTestIds.insert(t.id);
    for (const auto& c : req.centres) {
        for (const auto& tid : c.testIds) {
            if (!knownTestIds.count(tid)) {
                return errorResponse(400, "Bad Request",
                    "Centre '" + c.id + "' references unknown test '" + tid + "'");
            }
        }
    }

    QueueResult r = calculateStatelessQueue(req);

    map<string, string> testNames;
    for (const auto& t : req.tests) testNames[t.id] = t.name;
    map<string, string> centreNames;
    for (const auto& c : req.centres) centreNames[c.id] = c.name;

    auto testNameOf = [&](const string& id) -> string {
        auto it = testNames.find(id);
        return it != testNames.end() ? it->second : id;
    };
    auto centreNameOf = [&](const string& id) -> string {
        auto it = centreNames.find(id);
        return it != centreNames.end() ? it->second : id;
    };

    ostringstream j;
    j << "{\"success\":true,"
      << "\"computed\":" << (req.patients.empty() ? "false" : "true") << ","
      << "\"totalTime\":" << r.totalTime << ",";

    j << "\"events\":[";
    for (size_t i = 0; i < r.events.size(); ++i) {
        const auto& e = r.events[i];
        if (i) j << ",";
        j << "{\"type\":\"" << escapeJson(e.type) << "\""
          << ",\"time\":" << e.time
          << ",\"patientId\":\"" << escapeJson(e.patientId) << "\""
          << ",\"testId\":\"" << escapeJson(e.testId) << "\""
          << ",\"testName\":\"" << escapeJson(testNameOf(e.testId)) << "\""
          << ",\"centreId\":\"" << escapeJson(e.centreId) << "\""
          << ",\"centreName\":\"" << escapeJson(centreNameOf(e.centreId)) << "\"}";
    }
    j << "],";

    j << "\"perPatient\":{";
    bool firstP = true;
    for (const auto& kv : r.perPatient) {
        if (!firstP) j << ",";
        firstP = false;
        j << "\"" << escapeJson(kv.first) << "\":[";
        for (size_t i = 0; i < kv.second.size(); ++i) {
            if (i) j << ",";
            const auto& s = kv.second[i];
            j << "{\"testId\":\"" << escapeJson(s.testId) << "\""
              << ",\"testName\":\"" << escapeJson(testNameOf(s.testId)) << "\""
              << ",\"centreId\":\"" << escapeJson(s.centreId) << "\""
              << ",\"centreName\":\"" << escapeJson(centreNameOf(s.centreId)) << "\""
              << ",\"startTime\":" << s.startTime
              << ",\"duration\":" << s.duration
              << ",\"endTime\":" << (s.startTime + s.duration) << "}";
        }
        j << "]";
    }
    j << "},";

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

// [Change 3] Return actual configured state (no hard-coded
// machineCount:1 or enabled:true, and hospital name comes from
// ServerState, not Config::APP_NAME).
string handleGetConfiguration(const ServerState& st) {
    ostringstream j;
    j << "{\"success\":true,"
      << "\"hospitalId\":\"" << escapeJson(st.hospitalId) << "\","
      << "\"hospitalName\":\"" << escapeJson(st.hospitalName) << "\","
      << "\"tests\":[";
    for (size_t i = 0; i < st.tests.size(); ++i) {
        const auto& t = st.tests[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(t.id) << "\","
          << "\"name\":\"" << escapeJson(t.name) << "\","
          << "\"durationMinutes\":" << t.durationMinutes << ","
          << "\"bufferMinutes\":" << t.bufferMinutes << ","
          << "\"enabled\":" << (t.enabled ? "true" : "false") << "}";
    }
    j << "],\"centres\":[";
    for (size_t i = 0; i < st.centres.size(); ++i) {
        const auto& c = st.centres[i];
        if (i) j << ",";
        j << "{\"id\":\"" << escapeJson(c.id) << "\","
          << "\"name\":\"" << escapeJson(c.name) << "\","
          << "\"testId\":\"" << escapeJson(c.testId) << "\","
          << "\"testIds\":[\"" << escapeJson(c.testId) << "\"],"
          << "\"durationMinutes\":" << c.durationMinutes << ","
          << "\"bufferMinutes\":" << c.bufferMinutes << ","
          << "\"machineCount\":" << c.machineCount << ","
          << "\"totalMachines\":" << c.machineCount << ","
          << "\"activeMachines\":" << c.activeMachines << ","
          << "\"maintenanceMachines\":" << c.maintenanceMachines << ","
          << "\"currentActiveTests\":" << c.currentActiveTests << ","
          << "\"maxSimultaneousTests\":" << c.maxSimultaneousTests << ","
          << "\"enabled\":" << (c.enabled ? "true" : "false") << "}";
    }
    j << "]}";
    return jsonResponse(200, "OK", j.str());
}

// [Change 4] Accept and store the expanded fields. The old
// machineCount==1 restriction is gone. New fields default
// sensibly if omitted.
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

    // Optional hospital identity
    string incomingHospitalId = root.has("hospitalId") ? root.at("hospitalId").asStr("") : "";
    string incomingHospitalName = root.has("hospitalName") ? root.at("hospitalName").asStr("") : "";

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

        DiagnosticTest nt;
        nt.id = t.has("id") ? t.at("id").asStr("") : "";
        nt.name = t.has("name") ? t.at("name").asStr("") : "";
        if (t.has("durationMinutes") && t.at("durationMinutes").isNum())
            nt.durationMinutes = (int)t.at("durationMinutes").nval;
        if (t.has("bufferMinutes") && t.at("bufferMinutes").isNum())
            nt.bufferMinutes = (int)t.at("bufferMinutes").nval;
        if (t.has("enabled") && t.at("enabled").isBool())
            nt.enabled = t.at("enabled").bval;

        if (nt.id.empty())
            return errorResponse(400, "Bad Request", "Test 'id' is required");
        if (nt.name.empty())
            return errorResponse(400, "Bad Request", "Test 'name' is required (id=" + nt.id + ")");
        if (nt.durationMinutes <= 0)
            return errorResponse(400, "Bad Request",
                "Test duration must be positive (id=" + nt.id + ")");
        if (nt.bufferMinutes < 0)
            return errorResponse(400, "Bad Request",
                "Test buffer cannot be negative (id=" + nt.id + ")");
        if (testIds.count(nt.id))
            return errorResponse(400, "Bad Request", "Duplicate test id: " + nt.id);

        testIds.insert(nt.id);
        newTests.push_back(std::move(nt));
    }

    vector<TestingCentre> newCentres;
    set<string> centreIds;

    for (const auto& c : root.at("centres").arr) {
        if (!c.isObj())
            return errorResponse(400, "Bad Request", "Each centre must be an object");

        // Skip explicitly disabled entries only if they also lack an id
        bool rawEnabled = true;
        if (c.has("enabled") && c.at("enabled").isBool() && !c.at("enabled").bval)
            rawEnabled = false;

        TestingCentre nc;
        nc.id = c.has("id") ? c.at("id").asStr("") : "";
        nc.name = c.has("name") ? c.at("name").asStr("") : "";
        nc.testId = c.has("testId") ? c.at("testId").asStr("") : "";
        if (c.has("durationMinutes") && c.at("durationMinutes").isNum())
            nc.durationMinutes = (int)c.at("durationMinutes").nval;
        if (c.has("bufferMinutes") && c.at("bufferMinutes").isNum())
            nc.bufferMinutes = (int)c.at("bufferMinutes").nval;

        // [Change 2] New fields
        if (c.has("machineCount") && c.at("machineCount").isNum())
            nc.machineCount = (int)c.at("machineCount").nval;
        if (c.has("totalMachines") && c.at("totalMachines").isNum())
            nc.machineCount = (int)c.at("totalMachines").nval;
        if (c.has("activeMachines") && c.at("activeMachines").isNum())
            nc.activeMachines = (int)c.at("activeMachines").nval;
        if (c.has("maintenanceMachines") && c.at("maintenanceMachines").isNum())
            nc.maintenanceMachines = (int)c.at("maintenanceMachines").nval;
        if (c.has("currentActiveTests") && c.at("currentActiveTests").isNum())
            nc.currentActiveTests = (int)c.at("currentActiveTests").nval;
        if (c.has("maxSimultaneousTests") && c.at("maxSimultaneousTests").isNum())
            nc.maxSimultaneousTests = (int)c.at("maxSimultaneousTests").nval;

        nc.enabled = rawEnabled;
        if (c.has("status") && c.at("status").isStr()) {
            const string& s = c.at("status").sval;
            if (s == "INACTIVE" || s == "Inactive" || s == "inactive")
                nc.enabled = false;
        }

        if (nc.id.empty())
            return errorResponse(400, "Bad Request", "Centre 'id' is required");
        if (nc.name.empty())
            return errorResponse(400, "Bad Request", "Centre 'name' is required (id=" + nc.id + ")");
        if (nc.testId.empty())
            return errorResponse(400, "Bad Request", "Centre 'testId' is required (id=" + nc.id + ")");
        if (!testIds.count(nc.testId))
            return errorResponse(400, "Bad Request",
                "Centre '" + nc.id + "' references unknown test '" + nc.testId + "'");
        if (nc.durationMinutes <= 0)
            return errorResponse(400, "Bad Request",
                "Centre duration must be positive (id=" + nc.id + ")");
        if (nc.bufferMinutes < 0)
            return errorResponse(400, "Bad Request",
                "Centre buffer cannot be negative (id=" + nc.id + ")");
        if (nc.machineCount < 1)
            return errorResponse(400, "Bad Request",
                "Centre machineCount must be >= 1 (id=" + nc.id + ")");
        if (nc.activeMachines < 0 || nc.maintenanceMachines < 0)
            return errorResponse(400, "Bad Request",
                "Centre machine counts cannot be negative (id=" + nc.id + ")");
        if (nc.activeMachines + nc.maintenanceMachines > nc.machineCount)
            return errorResponse(400, "Bad Request",
                "Centre '" + nc.id + "': activeMachines + maintenanceMachines exceed machineCount");
        if (nc.maxSimultaneousTests < 1)
            return errorResponse(400, "Bad Request",
                "Centre maxSimultaneousTests must be >= 1 (id=" + nc.id + ")");
        if (nc.currentActiveTests < 0)
            return errorResponse(400, "Bad Request",
                "Centre currentActiveTests cannot be negative (id=" + nc.id + ")");
        if (centreIds.count(nc.id))
            return errorResponse(400, "Bad Request", "Duplicate centre id: " + nc.id);

        // Preserve historical counters if the centre already exists
        const TestingCentre* old = st.findCentre(nc.id);
        if (old) {
            nc.busyMinutes = old->busyMinutes;
            nc.completedCount = old->completedCount;
        }

        centreIds.insert(nc.id);
        newCentres.push_back(std::move(nc));
    }

    if (newCentres.empty())
        return errorResponse(400, "Bad Request",
            "Configuration must contain at least one centre");

    // Commit only after full validation succeeds.
    st.tests = std::move(newTests);
    st.centres = std::move(newCentres);
    if (!incomingHospitalId.empty())   st.hospitalId   = incomingHospitalId;
    if (!incomingHospitalName.empty()) st.hospitalName = incomingHospitalName;

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
        if (p == "/api/queue/calculate") return handleStatelessQueueCalculate(req.body);
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
        p == "/api/queue" || p == "/api/queue/calculate" ||
        p == "/api/queue/reset" || p == "/api/events" ||
        p == "/api/tests/complete" || p == "/api/configuration" ||
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
         << "[API] POST   /api/queue/calculate    (stateless)" << endl
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