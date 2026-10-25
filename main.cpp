#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// ============================================================
// PRATIKSHA
// Patient Routing & Intelligent Test Queue Handling System
// ============================================================

namespace Config {
    constexpr int PORT = 8080;
    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 16384;

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.0.0";
}

// ------------------------------------------------------------
// Diagnostic Test Model
// ------------------------------------------------------------

struct DiagnosticTest {
    string id;
    string name;
    int durationMinutes;
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
};

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
        << "HTTP/1.1 "
        << statusCode << " "
        << statusText << "\r\n"

        << "Content-Type: "
        << contentType << "\r\n"

        << "Content-Length: "
        << body.size() << "\r\n"

        << "Connection: close\r\n"

        << "Cache-Control: no-store\r\n"

        << "X-Content-Type-Options: nosniff\r\n"

        << "\r\n"

        << body;

    return response.str();
}

string jsonResponse(
    int statusCode,
    const string& statusText,
    const string& json
) {
    return makeResponse(
        statusCode,
        statusText,
        "application/json; charset=utf-8",
        json
    );
}

string errorResponse(
    int statusCode,
    const string& statusText,
    const string& message
) {
    // Current error messages are fixed internal strings.
    string body =
        "{\"success\":false,\"error\":\"" + message + "\"}";

    return jsonResponse(statusCode, statusText, body);
}

// ------------------------------------------------------------
// Send Complete HTTP Response
// ------------------------------------------------------------

bool sendAll(int clientSocket, const string& response) {
    size_t totalSent = 0;

    while (totalSent < response.size()) {
        ssize_t sent = send(
            clientSocket,
            response.data() + totalSent,
            response.size() - totalSent,
            0
        );

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }

            return false;
        }

        if (sent == 0) {
            return false;
        }

        totalSent += static_cast<size_t>(sent);
    }

    return true;
}

// ------------------------------------------------------------
// Read HTTP Request
// ------------------------------------------------------------

string readRequest(int clientSocket) {
    string request;
    char buffer[4096];

    while (request.size() < Config::MAX_REQUEST_SIZE) {
        ssize_t bytesRead = recv(
            clientSocket,
            buffer,
            sizeof(buffer),
            0
        );

        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }

            return "";
        }

        if (bytesRead == 0) {
            break;
        }

        request.append(buffer, static_cast<size_t>(bytesRead));

        // This first milestone handles GET requests without bodies.
        // Request-body parsing will be added with the patient API.
        if (request.find("\r\n\r\n") != string::npos) {
            break;
        }
    }

    return request;
}

// ------------------------------------------------------------
// HTTP Request-Line Parser
// ------------------------------------------------------------

struct HttpRequest {
    string method;
    string path;
    string version;
};

bool parseRequestLine(
    const string& request,
    HttpRequest& parsed
) {
    size_t lineEnd = request.find("\r\n");

    if (lineEnd == string::npos) {
        return false;
    }

    string firstLine = request.substr(0, lineEnd);
    istringstream input(firstLine);

    if (!(input >> parsed.method
                 >> parsed.path
                 >> parsed.version)) {
        return false;
    }

    string extra;

    if (input >> extra) {
        return false;
    }

    if (parsed.version != "HTTP/1.1" &&
        parsed.version != "HTTP/1.0") {
        return false;
    }

    return true;
}

// ------------------------------------------------------------
// API: Health Check
// ------------------------------------------------------------

string handleHealth() {
    return jsonResponse(
        200,
        "OK",
        "{"
            "\"success\":true,"
            "\"application\":\"PRATIKSHA\","
            "\"version\":\"1.0.0\","
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
        if (i > 0) {
            json << ",";
        }

        json
            << "{"
            << "\"id\":\"" << tests[i].id << "\","
            << "\"name\":\"" << tests[i].name << "\","
            << "\"durationMinutes\":"
            << tests[i].durationMinutes
            << "}";
    }

    json << "]}";

    return jsonResponse(200, "OK", json.str());
}

// ------------------------------------------------------------
// Serve Frontend
// ------------------------------------------------------------

string handleHome() {
    FILE* file = fopen("index.html", "rb");

    if (file == nullptr) {
        return errorResponse(
            500,
            "Internal Server Error",
            "index.html could not be opened"
        );
    }

    string html;
    char buffer[4096];

    while (true) {
        size_t bytesRead = fread(
            buffer,
            1,
            sizeof(buffer),
            file
        );

        html.append(buffer, bytesRead);

        if (bytesRead < sizeof(buffer)) {
            break;
        }
    }

    bool readFailed = ferror(file);
    fclose(file);

    if (readFailed) {
        return errorResponse(
            500,
            "Internal Server Error",
            "Failed to read index.html"
        );
    }

    return makeResponse(
        200,
        "OK",
        "text/html; charset=utf-8",
        html
    );
}

// ------------------------------------------------------------
// Route Requests
// ------------------------------------------------------------

string routeRequest(
    const HttpRequest& request,
    const TestRepository& repository
) {
    if (request.method != "GET") {
        return errorResponse(
            405,
            "Method Not Allowed",
            "Only GET requests are supported in this version"
        );
    }

    // Health endpoint
    if (request.path == "/api/health") {
        return handleHealth();
    }

    // Diagnostic test endpoint
    if (request.path == "/api/tests") {
        return handleTests(repository);
    }

    // Main frontend
    if (request.path == "/") {
        return handleHome();
    }

    // Unknown endpoint
    return errorResponse(
        404,
        "Not Found",
        "Requested route does not exist"
    );
}

// ------------------------------------------------------------
// Handle One Client
// ------------------------------------------------------------

void handleClient(
    int clientSocket,
    const TestRepository& repository
) {
    string rawRequest = readRequest(clientSocket);

    if (rawRequest.empty()) {
        string response = errorResponse(
            400,
            "Bad Request",
            "Empty or unreadable request"
        );

        sendAll(clientSocket, response);
        return;
    }

    HttpRequest request;

    if (!parseRequestLine(rawRequest, request)) {
        string response = errorResponse(
            400,
            "Bad Request",
            "Invalid HTTP request line"
        );

        sendAll(clientSocket, response);
        return;
    }

    // Do not accept query strings as part of route matching.
    size_t queryPosition = request.path.find('?');

    if (queryPosition != string::npos) {
        request.path = request.path.substr(0, queryPosition);
    }

    cout
        << "[REQUEST] "
        << request.method
        << " "
        << request.path
        << endl;

    string response = routeRequest(request, repository);

    sendAll(clientSocket, response);
}

// ------------------------------------------------------------
// Create HTTP Server Socket
// ------------------------------------------------------------

int createServerSocket() {
    int serverSocket = socket(
        AF_INET,
        SOCK_STREAM,
        0
    );

    if (serverSocket < 0) {
        cerr << "[ERROR] socket(): "
             << strerror(errno) << endl;

        return -1;
    }

    int reuseAddress = 1;

    if (setsockopt(
            serverSocket,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)
        ) < 0) {

        cerr << "[ERROR] setsockopt(): "
             << strerror(errno) << endl;

        close(serverSocket);
        return -1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(Config::PORT);

    // Listen only on this Mac, not on the public network.
    serverAddress.sin_addr.s_addr =
        htonl(INADDR_LOOPBACK);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) < 0) {

        cerr << "[ERROR] bind(): "
             << strerror(errno) << endl;

        close(serverSocket);
        return -1;
    }

    if (listen(
            serverSocket,
            Config::BACKLOG
        ) < 0) {

        cerr << "[ERROR] listen(): "
             << strerror(errno) << endl;

        close(serverSocket);
        return -1;
    }

    return serverSocket;
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main() {
    cout << endl;
    cout << "========================================" << endl;
    cout << "       PRATIKSHA HOSPITAL SYSTEM" << endl;
    cout << "========================================" << endl;

    TestRepository testRepository;

    int serverSocket = createServerSocket();

    if (serverSocket < 0) {
        cerr << "[FATAL] Server could not start." << endl;
        return 1;
    }

    cout << "[SUCCESS] C++ server started." << endl;

    cout
        << "[SERVER] http://127.0.0.1:"
        << Config::PORT
        << endl;

    cout
        << "[API] http://127.0.0.1:"
        << Config::PORT
        << "/api/health"
        << endl;

    cout
        << "[API] http://127.0.0.1:"
        << Config::PORT
        << "/api/tests"
        << endl;

    cout << "[INFO] Press Control+C to stop." << endl;
    cout << "========================================" << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLength = sizeof(clientAddress);

        int clientSocket = accept(
            serverSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLength
        );

        if (clientSocket < 0) {
            if (errno == EINTR) {
                continue;
            }

            cerr << "[ERROR] accept(): "
                 << strerror(errno) << endl;

            continue;
        }

        handleClient(clientSocket, testRepository);

        shutdown(clientSocket, SHUT_RDWR);
        close(clientSocket);
    }

    close(serverSocket);

    return 0;
}