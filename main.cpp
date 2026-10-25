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
// PRATIKSHA
// Patient Routing & Intelligent Test Queue Handling System
// Backend v1.1 — adds Patient API + Smart Queue
// ============================================================
//
// API Endpoints:
//   GET    /                     -> index.html
//   GET    /api/health           -> health check
//   GET    /api/tests            -> list diagnostic tests
//   GET    /api/centres          -> list test centres (static)
//   GET    /api/patients         -> list queued patients
//   POST   /api/patients         -> add patient {id,name,tests[]}
//   DELETE /api/patients/{id}    -> remove patient
//   GET    /api/queue            -> compute + return queue
//   POST   /api/queue/reset      -> clear all patients
// ============================================================

namespace Config {
    constexpr int PORT = 8080;
    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 32768;
    constexpr int QUEUE_MAX_TIME = 5000;   // safety cap for simulation

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.1.0";
}

// ------------------------------------------------------------
// Models
// ------------------------------------------------------------

struct DiagnosticTest {
    string id;
    string name;
    int durationMinutes;
};

struct Patient {
    string id;
    string name;
    vector<string> tests;   // test IDs (unique, validated)
    long addedSeq;
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
    string currentPatient;   // empty if free
    int remainingMin;
    int busyMinutes;
    int completedCount;
};

struct QueueResult {
    int totalTime;
    vector<CentreState> centres;
    map<string, vector<QueueStep>> perPatient;
    vector<QueueEvent> events;
};

// ------------------------------------------------------------
// Test Repository
// ------------------------------------------------------------

class TestRepository {
private:
    vector<DiagnosticTest> tests;

public:
    TestRepository() {
        tests.push_back({"A", "X-Ray", 10});
        tests.push_back({"B", "Blood Test", 5});
        tests.push_back({"C", "Ultrasound", 20});
        tests.push_back({"D", "CT Scan", 30});
    }

    const vector<DiagnosticTest>& getAll() const {
        return tests;
    }

    bool exists(const string& id) const {
        for (const auto& t : tests) {
            if (t.id == id) return true;
        }
        return false;
    }

    const DiagnosticTest* find(const string& id) const {
        for (const auto& t : tests) {
            if (t.id == id) return &t;
        }
        return nullptr;
    }
};

// ------------------------------------------------------------
// Server State
// ------------------------------------------------------------

class ServerState {
public:
    TestRepository testRepository;
    vector<Patient> patients;
    long nextSeq = 0;

    bool hasPatient(const string& id) const {
        for (const auto& p : patients) {
            if (p.id == id) return true;
        }
        return false;
    }

    bool addPatient(const string& id, const string& name, const vector<string>& tests) {
        if (id.empty()) return false;
        if (hasPatient(id)) return false;

        // Validate + dedupe tests
        vector<string> uniqueTests;
        set<string> seen;
        for (const auto& t : tests) {
            if (!testRepository.exists(t)) return false;
            if (seen.count(t)) continue;
            seen.insert(t);
            uniqueTests.push_back(t);
        }

        if (uniqueTests.empty()) return false;

        patients.push_back({
            id,
            name,
            uniqueTests,
            nextSeq++
        });
        return true;
    }

    bool removePatient(const string& id) {
        for (size_t i = 0; i < patients.size(); ++i) {
            if (patients[i].id == id) {
                patients.erase(patients.begin() + i);
                // Re-index seq to keep ordering stable
                for (size_t j = 0; j < patients.size(); ++j) {
                    patients[j].addedSeq = (long)j;
                }
                return true;
            }
        }
        return false;
    }

    void reset() {
        patients.clear();
        nextSeq = 0;
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

// Very small, purpose-built parser for flat request bodies.
// Handles:  {"id":"P1","name":"Amit","tests":["A","B"]}
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

// ------------------------------------------------------------
// HTTP Response Helpers
// ------------------------------------------------------------

string makeResponse(
    int statusCode,
    const string& statusText,
    const string& contentType,
    const string& body
) {
    ostringstream response;
    response
        << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "Cache-Control: no-store\r\n"
        << "X-Content-Type-Options: nosniff\r\n"
        << "\r\n"
        << body;
    return response.str();
}

string jsonResponse(int statusCode, const string& statusText, const string& json) {
    return makeResponse(
        statusCode, statusText,
        "application/json; charset=utf-8",
        json
    );
}

string errorResponse(int statusCode, const string& statusText, const string& message) {
    string body =
        "{\"success\":false,\"error\":\"" + escapeJson(message) + "\"}";
    return jsonResponse(statusCode, statusText, body);
}

// ------------------------------------------------------------
// Send Complete HTTP Response
// ------------------------------------------------------------

bool sendAll(int clientSocket, const string& response) {
    size_t totalSent = 0;
    while (totalSent < response.size()) {
        ssize_t sent = ::send(
            clientSocket,
            response.data() + totalSent,
            response.size() - totalSent,
            0
        );
        if (sent < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (sent == 0) return false;
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

// ------------------------------------------------------------
// Read HTTP Request (now supports POST bodies)
// ------------------------------------------------------------

long parseContentLength(const string& headers) {
    size_t pos = 0;
    while ((pos = headers.find("Content-Length:", pos)) != string::npos) {
        bool lineStart = (pos == 0) || (headers[pos - 1] == '\n');
        if (lineStart) {
            size_t valStart = pos + 15;
            size_t valEnd = headers.find("\r\n", valStart);
            if (valEnd == string::npos) valEnd = headers.size();
            string valStr = headers.substr(valStart, valEnd - valStart);
            while (!valStr.empty() && isspace((unsigned char)valStr.front())) valStr.erase(0, 1);
            while (!valStr.empty() && isspace((unsigned char)valStr.back()))  valStr.pop_back();
            try { return stol(valStr); } catch (...) { return -1; }
        }
        pos++;
    }
    return -1;
}

string readRequest(int clientSocket) {
    string request;
    char buffer[4096];
    size_t headerEnd = string::npos;
    long contentLength = -1;

    while (request.size() < Config::MAX_REQUEST_SIZE) {
        if (headerEnd != string::npos) {
            size_t bodyStart = headerEnd + 4;
            size_t bodyReceived = request.size() > bodyStart
                ? request.size() - bodyStart : 0;
            if (contentLength <= 0 ||
                bodyReceived >= static_cast<size_t>(contentLength)) {
                break;
            }
        }

        ssize_t bytesRead = ::recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead < 0) {
            if (errno == EINTR) continue;
            return "";
        }
        if (bytesRead == 0) break;

        request.append(buffer, static_cast<size_t>(bytesRead));

        if (headerEnd == string::npos) {
            headerEnd = request.find("\r\n\r\n");
            if (headerEnd != string::npos) {
                contentLength = parseContentLength(
                    request.substr(0, headerEnd)
                );
            }
        }
    }

    return request;
}

// ------------------------------------------------------------
// HTTP Request Parsing
// ------------------------------------------------------------

struct HttpRequest {
    string method;
    string path;
    string version;
    string headers;
    string body;
};

bool parseRequestLine(const string& request, HttpRequest& parsed) {
    size_t lineEnd = request.find("\r\n");
    if (lineEnd == string::npos) return false;

    string firstLine = request.substr(0, lineEnd);
    istringstream input(firstLine);
    if (!(input >> parsed.method >> parsed.path >> parsed.version)) return false;

    string extra;
    if (input >> extra) return false;

    if (parsed.version != "HTTP/1.1" &&
        parsed.version != "HTTP/1.0") return false;

    // Split headers/body
    size_t headerEnd = request.find("\r\n\r\n");
    if (headerEnd != string::npos) {
        parsed.headers = request.substr(lineEnd + 2, headerEnd - (lineEnd + 2));
        parsed.body = request.substr(headerEnd + 4);
    }

    return true;
}

bool matchPrefix(const string& path, const string& prefix, string& remainder) {
    if (path.size() > prefix.size() &&
        path.compare(0, prefix.size(), prefix) == 0) {
        remainder = path.substr(prefix.size());
        return true;
    }
    return false;
}

// ------------------------------------------------------------
// Queue Algorithm
// ------------------------------------------------------------

QueueResult computeQueue(
    const vector<Patient>& patients,
    const TestRepository& repository
) {
    QueueResult result;

    // Snapshot patient working copies
    struct WorkPatient {
        string id;
        vector<string> remaining;
        long addedSeq;
    };
    vector<WorkPatient> work;
    for (const auto& p : patients) {
        work.push_back({p.id, p.tests, p.addedSeq});
        result.perPatient[p.id] = {};
    }

    // Snapshot centres
    for (const auto& t : repository.getAll()) {
        CentreState c;
        c.id = t.id;
        c.name = t.name;
        c.duration = t.durationMinutes;
        c.currentPatient = "";
        c.remainingMin = 0;
        c.busyMinutes = 0;
        c.completedCount = 0;
        result.centres.push_back(c);
    }

    int time = 0;
    while (time < Config::QUEUE_MAX_TIME) {
        // ---- 1) Process completions ----
        for (auto& c : result.centres) {
            if (!c.currentPatient.empty() && c.remainingMin <= 0) {
                for (auto& p : work) {
                    if (p.id == c.currentPatient) {
                        auto it = std::find(
                            p.remaining.begin(), p.remaining.end(), c.id
                        );
                        if (it != p.remaining.end()) {
                            p.remaining.erase(it);
                        }
                        break;
                    }
                }
                result.events.push_back({
                    time, "complete", c.id, c.currentPatient
                });
                c.completedCount++;
                c.currentPatient = "";
            }
        }

        // ---- 2) All done? ----
        bool allDone = true;
        for (const auto& p : work) {
            if (!p.remaining.empty()) { allDone = false; break; }
        }
        if (allDone) break;

        // ---- 3) Assign free centres ----
        set<string> busyPatients;
        for (const auto& c : result.centres) {
            if (!c.currentPatient.empty()) busyPatients.insert(c.currentPatient);
        }

        for (auto& c : result.centres) {
            if (!c.currentPatient.empty()) continue;

            // Eligible = patient needs this test + not already busy
            vector<WorkPatient*> eligible;
            for (auto& p : work) {
                if (busyPatients.count(p.id)) continue;
                if (std::find(p.remaining.begin(),
                              p.remaining.end(),
                              c.id) == p.remaining.end()) continue;
                eligible.push_back(&p);
            }

            if (eligible.empty()) continue;

            // Priority: waitingMinutes + testDuration (higher = better)
            auto score = [&](const WorkPatient* p) {
                long wait = time - p->addedSeq;   // addedSeq used as arrival slot
                if (wait < 0) wait = 0;
                return wait + c.duration;
            };
            std::stable_sort(eligible.begin(), eligible.end(),
                [&](const WorkPatient* a, const WorkPatient* b) {
                    return score(a) > score(b);
                });

            WorkPatient* chosen = eligible.front();
            c.currentPatient = chosen->id;
            c.remainingMin = c.duration;
            busyPatients.insert(chosen->id);

            result.events.push_back({
                time, "start", c.id, chosen->id
            });
            result.perPatient[chosen->id].push_back({
                c.id, time, c.duration
            });
        }

        // ---- 4) Advance time by 1 minute ----
        time++;
        for (auto& c : result.centres) {
            if (c.remainingMin > 0) {
                c.remainingMin--;
                c.busyMinutes++;
            }
        }
    }

    result.totalTime = time;
    return result;
}

// ------------------------------------------------------------
// API: Health Check
// ------------------------------------------------------------

string handleHealth() {
    return jsonResponse(200, "OK",
        "{"
            "\"success\":true,"
            "\"application\":\"PRATIKSHA\","
            "\"version\":\"1.1.0\","
            "\"status\":\"running\","
            "\"backend\":\"C++17\""
        "}"
    );
}

// ------------------------------------------------------------
// API: Diagnostic Tests
// ------------------------------------------------------------

string handleTests(const TestRepository& repository) {
    ostringstream json;
    json << "{\"success\":true,\"tests\":[";
    const auto& tests = repository.getAll();
    for (size_t i = 0; i < tests.size(); ++i) {
        if (i > 0) json << ",";
        json
            << "{"
            << "\"id\":\"" << escapeJson(tests[i].id) << "\","
            << "\"name\":\"" << escapeJson(tests[i].name) << "\","
            << "\"durationMinutes\":" << tests[i].durationMinutes
            << "}";
    }
    json << "]}";
    return jsonResponse(200, "OK", json.str());
}

// ------------------------------------------------------------
// API: Centres
// ------------------------------------------------------------

string handleCentres(const TestRepository& repository) {
    ostringstream json;
    json << "{\"success\":true,\"centres\":[";
    const auto& tests = repository.getAll();
    for (size_t i = 0; i < tests.size(); ++i) {
        if (i > 0) json << ",";
        json
            << "{"
            << "\"id\":\"" << escapeJson(tests[i].id) << "\","
            << "\"name\":\"" << escapeJson(tests[i].name) << "\","
            << "\"duration\":" << tests[i].durationMinutes
            << "}";
    }
    json << "]}";
    return jsonResponse(200, "OK", json.str());
}

// ------------------------------------------------------------
// API: Patients
// ------------------------------------------------------------

string handleListPatients(const ServerState& state) {
    ostringstream json;
    json << "{\"success\":true,\"patients\":[";
    for (size_t i = 0; i < state.patients.size(); ++i) {
        const auto& p = state.patients[i];
        if (i > 0) json << ",";
        json
            << "{"
            << "\"id\":\"" << escapeJson(p.id) << "\","
            << "\"name\":\"" << escapeJson(p.name) << "\","
            << "\"tests\":[";
        for (size_t j = 0; j < p.tests.size(); ++j) {
            if (j > 0) json << ",";
            json << "\"" << escapeJson(p.tests[j]) << "\"";
        }
        json << "]}";
    }
    json << "]}";
    return jsonResponse(200, "OK", json.str());
}

string handleAddPatient(ServerState& state, const string& body) {
    if (body.empty()) {
        return errorResponse(400, "Bad Request", "Request body is empty");
    }

    string id   = jsonGetString(body, "id");
    string name = jsonGetString(body, "name");
    vector<string> tests = jsonGetStringArray(body, "tests");

    if (id.empty()) {
        return errorResponse(400, "Bad Request", "Patient 'id' is required");
    }
    if (tests.empty()) {
        return errorResponse(400, "Bad Request", "At least one test is required");
    }

    // Validate tests
    for (const auto& t : tests) {
        if (!state.testRepository.exists(t)) {
            string msg = "Unknown test ID: " + t;
            return errorResponse(400, "Bad Request", msg);
        }
    }

    if (!state.addPatient(id, name, tests)) {
        if (state.hasPatient(id)) {
            return errorResponse(409, "Conflict", "Patient ID already exists");
        }
        return errorResponse(400, "Bad Request", "Could not add patient");
    }

    // Echo back created patient
    ostringstream json;
    json
        << "{\"success\":true,\"patient\":{"
        << "\"id\":\"" << escapeJson(id) << "\","
        << "\"name\":\"" << escapeJson(name) << "\","
        << "\"tests\":[";
    const auto& added = state.patients.back();
    for (size_t j = 0; j < added.tests.size(); ++j) {
        if (j > 0) json << ",";
        json << "\"" << escapeJson(added.tests[j]) << "\"";
    }
    json << "]}}";

    return jsonResponse(201, "Created", json.str());
}

string handleDeletePatient(ServerState& state, const string& id) {
    if (id.empty()) {
        return errorResponse(400, "Bad Request", "Patient ID is required");
    }
    if (!state.removePatient(id)) {
        return errorResponse(404, "Not Found", "Patient not found");
    }
    ostringstream json;
    json << "{\"success\":true,\"removed\":\"" << escapeJson(id) << "\"}";
    return jsonResponse(200, "OK", json.str());
}

// ------------------------------------------------------------
// API: Queue
// ------------------------------------------------------------

string handleGetQueue(const ServerState& state) {
    QueueResult result = computeQueue(state.patients, state.testRepository);

    ostringstream json;
    json << "{\"success\":true,";
    json << "\"computed\":" << (state.patients.empty() ? "false" : "true") << ",";
    json << "\"totalTime\":" << result.totalTime << ",";

    // Patients
    json << "\"patients\":[";
    for (size_t i = 0; i < state.patients.size(); ++i) {
        const auto& p = state.patients[i];
        if (i > 0) json << ",";
        json
            << "{"
            << "\"id\":\"" << escapeJson(p.id) << "\","
            << "\"name\":\"" << escapeJson(p.name) << "\""
            << "}";
    }
    json << "],";

    // Centres
    json << "\"centres\":[";
    for (size_t i = 0; i < result.centres.size(); ++i) {
        const auto& c = result.centres[i];
        if (i > 0) json << ",";
        int util = result.totalTime > 0
            ? (int)((c.busyMinutes * 100.0) / result.totalTime)
            : 0;
        json
            << "{"
            << "\"id\":\"" << escapeJson(c.id) << "\","
            << "\"name\":\"" << escapeJson(c.name) << "\","
            << "\"duration\":" << c.duration << ","
            << "\"busyMinutes\":" << c.busyMinutes << ","
            << "\"completedCount\":" << c.completedCount << ","
            << "\"utilization\":" << util
            << "}";
    }
    json << "],";

    // perPatient
    json << "\"perPatient\":{";
    bool firstPatient = true;
    for (const auto& kv : result.perPatient) {
        if (!firstPatient) json << ",";
        firstPatient = false;
        json << "\"" << escapeJson(kv.first) << "\":[";
        for (size_t i = 0; i < kv.second.size(); ++i) {
            if (i > 0) json << ",";
            const auto& s = kv.second[i];
            json
                << "{"
                << "\"testId\":\"" << escapeJson(s.testId) << "\","
                << "\"startTime\":" << s.startTime << ","
                << "\"duration\":" << s.duration
                << "}";
        }
        json << "]";
    }
    json << "},";

    // events
    json << "\"events\":[";
    for (size_t i = 0; i < result.events.size(); ++i) {
        if (i > 0) json << ",";
        const auto& e = result.events[i];
        json
            << "{"
            << "\"time\":" << e.time << ","
            << "\"type\":\"" << escapeJson(e.type) << "\","
            << "\"centreId\":\"" << escapeJson(e.centreId) << "\","
            << "\"patientId\":\"" << escapeJson(e.patientId) << "\""
            << "}";
    }
    json << "]";

    json << "}";
    return jsonResponse(200, "OK", json.str());
}

string handleResetQueue(ServerState& state) {
    state.reset();
    return jsonResponse(200, "OK",
        "{\"success\":true,\"message\":\"Queue cleared\"}"
    );
}

// ------------------------------------------------------------
// Serve Frontend
// ------------------------------------------------------------

string handleHome() {
    FILE* file = fopen("index.html", "rb");
    if (file == nullptr) {
        return errorResponse(500, "Internal Server Error",
            "index.html could not be opened");
    }

    string html;
    char buffer[4096];
    while (true) {
        size_t bytesRead = fread(buffer, 1, sizeof(buffer), file);
        html.append(buffer, bytesRead);
        if (bytesRead < sizeof(buffer)) break;
    }
    bool readFailed = ferror(file);
    fclose(file);

    if (readFailed) {
        return errorResponse(500, "Internal Server Error",
            "Failed to read index.html");
    }
    return makeResponse(200, "OK",
        "text/html; charset=utf-8", html);
}

// ------------------------------------------------------------
// Route Requests
// ------------------------------------------------------------

string routeRequest(
    const HttpRequest& request,
    ServerState& state
) {
    const string& method = request.method;
    const string& path   = request.path;

    // ---- Public GETs ----
    if (method == "GET") {
        if (path == "/")            return handleHome();
        if (path == "/api/health")  return handleHealth();
        if (path == "/api/tests")   return handleTests(state.testRepository);
        if (path == "/api/centres") return handleCentres(state.testRepository);
        if (path == "/api/patients") return handleListPatients(state);
        if (path == "/api/queue")   return handleGetQueue(state);
    }

    // ---- POSTs ----
    if (method == "POST") {
        if (path == "/api/patients") {
            return handleAddPatient(state, request.body);
        }
        if (path == "/api/queue/reset") {
            return handleResetQueue(state);
        }
    }

    // ---- DELETEs ----
    if (method == "DELETE") {
        string remainder;
        if (matchPrefix(path, "/api/patients/", remainder)) {
            return handleDeletePatient(state, remainder);
        }
    }

    // ---- Route exists but method not allowed? ----
    bool knownPath =
        path == "/" ||
        path == "/api/health" ||
        path == "/api/tests" ||
        path == "/api/centres" ||
        path == "/api/patients" ||
        path == "/api/queue" ||
        path.rfind("/api/patients/", 0) == 0;

    if (knownPath) {
        return errorResponse(405, "Method Not Allowed",
            "Method not allowed for this route");
    }

    return errorResponse(404, "Not Found",
        "Requested route does not exist");
}

// ------------------------------------------------------------
// Handle One Client
// ------------------------------------------------------------

void handleClient(int clientSocket, ServerState& state) {
    string rawRequest = readRequest(clientSocket);

    if (rawRequest.empty()) {
        sendAll(clientSocket,
            errorResponse(400, "Bad Request",
                "Empty or unreadable request"));
        return;
    }

    HttpRequest request;
    if (!parseRequestLine(rawRequest, request)) {
        sendAll(clientSocket,
            errorResponse(400, "Bad Request",
                "Invalid HTTP request line"));
        return;
    }

    // Strip query string from path for routing
    size_t queryPos = request.path.find('?');
    if (queryPos != string::npos) {
        request.path = request.path.substr(0, queryPos);
    }

    cout << "[REQUEST] " << request.method
         << " " << request.path
         << " (" << request.body.size() << " body bytes)"
         << endl;

    string response = routeRequest(request, state);
    sendAll(clientSocket, response);
}

// ------------------------------------------------------------
// Create HTTP Server Socket
// ------------------------------------------------------------

int createServerSocket() {
    int serverSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "[ERROR] socket(): " << strerror(errno) << endl;
        return -1;
    }

    int reuseAddress = 1;
    if (::setsockopt(
            serverSocket, SOL_SOCKET, SO_REUSEADDR,
            &reuseAddress, sizeof(reuseAddress)
        ) < 0) {
        cerr << "[ERROR] setsockopt(): " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(Config::PORT);
    serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (::bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) < 0) {
        cerr << "[ERROR] bind(): " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    if (::listen(serverSocket, Config::BACKLOG) < 0) {
        cerr << "[ERROR] listen(): " << strerror(errno) << endl;
        close(serverSocket);
        return -1;
    }

    return serverSocket;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main() {
    signal(SIGPIPE, SIG_IGN);

    cout << endl;
    cout << "========================================" << endl;
    cout << "       PRATIKSHA HOSPITAL SYSTEM" << endl;
    cout << "              v" << Config::VERSION << endl;
    cout << "========================================" << endl;

    ServerState state;
    int serverSocket = createServerSocket();
    if (serverSocket < 0) {
        cerr << "[FATAL] Server could not start." << endl;
        return 1;
    }

    cout << "[SUCCESS] C++ server started." << endl;
    cout << "[SERVER] http://127.0.0.1:" << Config::PORT << endl;
    cout << "[API] GET    /api/health"          << endl;
    cout << "[API] GET    /api/tests"           << endl;
    cout << "[API] GET    /api/centres"         << endl;
    cout << "[API] GET    /api/patients"        << endl;
    cout << "[API] POST   /api/patients"        << endl;
    cout << "[API] DELETE /api/patients/{id}"   << endl;
    cout << "[API] GET    /api/queue"           << endl;
    cout << "[API] POST   /api/queue/reset"     << endl;
    cout << "[INFO] Press Control+C to stop."   << endl;
    cout << "========================================" << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = ::accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (clientSocket < 0) {
            if (errno == EINTR) continue;
            cerr << "[ERROR] accept(): " << strerror(errno) << endl;
            continue;
        }

        handleClient(clientSocket, state);

        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
    }

    close(serverSocket);
    return 0;
}