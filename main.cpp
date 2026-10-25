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

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// ============================================================
// PRATIKSHA — Patient Routing & Intelligent Test Queue Handling
// Backend v1.6
//
// Changes over v1.5:
//   [Step 1] Automatic patient ID generation (P001, P002, ...)
//            - handleAddPatient no longer requires "id"
//            - ServerState keeps nextPatientNumber (never reused on delete)
//            - /api/patients exposes nextPatientId for frontend
//
//   [Step 2] Single machine per testing centre.
//            Default centres: A1, B1, C1, D1 (no more A2).
//
//   [Step 3] POST /api/configuration — import hospital config
//            from JSON. Uses a small self-contained JSON parser
//            so nested objects/arrays are safe. Validates:
//              - non-empty tests + centres
//              - unique test & centre IDs
//              - each centre.testId exists
//              - durations > 0
//              - machineCount == 1
//            Rejects import while patients exist or centres busy.
//
//   [Step 4] Live timing per centre:
//              - startedAtMs, expectedCompletionMs (steady_clock)
//              - /api/centres reports status
//                (AVAILABLE / BUSY / AWAITING_CONFIRMATION),
//                elapsedSeconds, remainingSeconds
//              - handleNextPatient stamps timestamps
//              - handleCompleteTest clears timestamps
//              - computeQueue respects live remaining time
// ============================================================

namespace Config {
    constexpr int PORT = 8080;
    constexpr int BACKLOG = 10;
    constexpr size_t MAX_REQUEST_SIZE = 131072; // 128KB (config uploads)
    constexpr int QUEUE_MAX_TIME = 5000;
    constexpr long INF_TIME = 1000000000L;
    constexpr double LAMBDA = 0.5;
    constexpr int MOVE_TIME_MINUTES = 3;

    const string APP_NAME = "PRATIKSHA";
    const string VERSION = "1.6.0";
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