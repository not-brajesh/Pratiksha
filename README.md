# PRATIKSHA — Smart Diagnostic Queue Management System

**A C++17-powered diagnostic queue management system designed to coordinate patients, diagnostic testing centres, and estimated testing schedules through a real-time dashboard.**

<p align="center">
  <strong>Live Demo:</strong>
  <a href="https://pratiksha.onrender.com">pratiksha.onrender.com</a>
</p>

---

## Overview

PRATIKSHA is a diagnostic queue management application designed to simplify patient routing and diagnostic test scheduling.

The application combines a browser-based dashboard with a C++ backend to display patient information, testing centre availability, queue status, and estimated completion timelines.

The goal is to make diagnostic workflows easier to monitor and manage through a single interface.

## Key Features

- **Centralised Dashboard** — View patient statistics, pending tests, testing centre status, and estimated completion time.
- **Patient Management** — Register and manage patients through the dashboard.
- **Diagnostic Centre Monitoring** — View testing centres, machine availability, and current activity.
- **Queue Scheduling** — Monitor the diagnostic queue and upcoming schedule.
- **Automatic and Manual Operation** — Support for automatic assignment workflows and manual control.
- **Live Timeline** — Display time-based scheduling information using the backend.
- **IST Clock** — Show the current date and time in Indian Standard Time.
- **REST API** — Provide backend endpoints for patient, queue, testing centre, and configuration operations.
- **Docker Support** — Package the C++ application and frontend for deployment.
- **Cloud Deployment** — Run the application using Render.

> **Note:** Feature availability in the deployed version should be verified against the current source code and live application. Data persistence across server restarts has not yet been confirmed.

## Live Application

**Website:** https://pratiksha.onrender.com

**Backend Health Check:** https://pratiksha.onrender.com/api/health

The health endpoint currently reports the application name, version, backend technology, running status, and timezone.

## Technology Stack

| Technology | Purpose |
|---|---|
| C++17 | Backend application and queue-processing logic |
| HTML, CSS and JavaScript | Browser-based dashboard |
| HTTP/REST API | Communication between the frontend and backend |
| Docker | Application containerisation |
| Ubuntu 24.04 | Container base image |
| Render | Cloud hosting and deployment |
| Git and GitHub | Version control and source code hosting |

## Application Dashboard

The dashboard provides a central overview of diagnostic operations.

Current dashboard sections include:

- Overview
- Patient Queue
- Testing Centres
- Queue Schedule

The overview displays patient totals, pending tests, machine utilisation, estimated completion time, testing centre activity, and upcoming scheduling information.

### Screenshot

Add your dashboard screenshot to the repository at `docs/images/dashboard.png`, then display it here:

```markdown
![PRATIKSHA Dashboard](docs/images/dashboard.png)
```

## Architecture

PRATIKSHA follows a simple client-server architecture.

```text
                USER
                  |
                  v
       Browser-Based Dashboard
          HTML / CSS / JS
                  |
                  | HTTP Requests
                  v
          C++17 Backend
                  |
          +-------+--------+
          |       |        |
          v       v        v
       Patients  Centres   Queue
          |       |        |
          +-------+--------+
                  |
                  v
         Scheduling Response
                  |
                  v
        Dashboard Updates
```

The frontend displays information and sends requests to the backend. The backend processes requests and returns responses to the dashboard.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for additional details.

## Getting Started

### Prerequisites

Install the following tools:

- Git
- A C++17-compatible compiler, such as `g++` or `clang++`
- Docker Desktop (optional, for containerised execution)

### 1. Clone the Repository

```bash
git clone https://github.com/not-brajesh/Pratiksha.git
cd Pratiksha
```

### 2. Build the Application

On a system with `g++` installed:

```bash
g++ -std=c++17 -O2 -pthread main.cpp -o hospital
```

### 3. Run the Application

```bash
./hospital
```

The current deployment configuration uses port `8080`. Open the local address reported by the application in your browser:

```text
http://127.0.0.1:8080
```

If your local configuration uses a different port, use the address printed by the server.

### 4. Verify the Backend

Open:

```text
http://127.0.0.1:8080/api/health
```

A successful response should report that the application is running.

## Docker Deployment

The repository contains a `Dockerfile` for building the application in an Ubuntu-based container.

### Build the Image

```bash
docker build -t pratiksha-backend .
```

### Run the Container

```bash
docker run --rm -p 8080:8080 -e PORT=8080 pratiksha-backend
```

### Verify the Container

Open:

```text
http://127.0.0.1:8080/api/health
```

**Important:** Ensure that `Dockerfile` copies both `main.cpp` and `index.html` into the image. The backend must be able to locate `index.html` at runtime.

## API Overview

The application exposes endpoints for health checks, patient management, testing centres, queue operations, and configuration.

The following routes have been observed in the current application logs:

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/api/health` | Check backend status |
| GET | `/api/tests` | Retrieve test information |
| GET | `/api/centres` | Retrieve testing centres |
| GET | `/api/centres/{id}/next` | Centre-specific next-patient operation |
| GET | `/api/patients` | Retrieve patients |
| POST | `/api/patients` | Register a patient |
| DELETE | `/api/patients/{id}` | Delete a patient |
| POST | `/api/tests/complete` | Complete a test |
| GET | `/api/queue` | Retrieve queue information |
| POST | `/api/queue/reset` | Reset queue state |
| GET | `/api/events` | Retrieve event history |
| GET | `/api/configuration` | Retrieve configuration |
| POST | `/api/configuration` | Update or import configuration |

For complete request formats, validation rules, and response examples, consult [`docs/API.md`](docs/API.md) after verifying the implementation in `main.cpp`.

## Deployment

PRATIKSHA is deployed using Render and Docker.

The repository is connected to the deployment service, allowing source code changes to be built and deployed through the configured deployment workflow.

Live URL:

https://pratiksha.onrender.com

See [`docs/DEPLOYMENT.md`](docs/DEPLOYMENT.md) for deployment notes and troubleshooting.

## Project Structure

```text
Pratiksha/
├── main.cpp
├── index.html
├── Dockerfile
├── autocommit.sh
├── docs/
│   ├── ARCHITECTURE.md
│   ├── API.md
│   ├── DEPLOYMENT.md
│   ├── USER_GUIDE.md
│   └── images/
└── README.md
```

This is the intended documentation layout. Add the `docs` files and directory as you create them. Other repository files, such as `hospital` and `hospital.backup`, may exist locally and are not necessarily part of the tracked project.

## Roadmap

Potential future improvements include:

- Persistent database storage
- More detailed queue optimisation
- Improved scheduling and wait-time estimation
- Patient search and filtering
- Exportable reports
- Stronger validation and error handling
- Automated testing and continuous integration
- Authentication and role-based access control
- Improved monitoring and deployment diagnostics

These are proposed improvements, not claims that the features are already implemented.

## Reliability and Data Safety

PRATIKSHA is a software project and should not be treated as a production-ready clinical system without further validation.

Before using it with real patient information, consider:

- Persistent and recoverable data storage
- Authentication and access control
- Protection of sensitive patient information
- Input validation and audit logging
- Backup and recovery procedures
- Testing under concurrent requests
- Validation of scheduling and queue-allocation logic

Do not use real patient data in a public demonstration.

## Contributing

Contributions and suggestions are welcome.

1. Fork the repository.
2. Create a feature branch.
3. Implement and test your changes.
4. Submit a pull request describing the changes.

## Author

**Brajesh Kumar**

GitHub: [@not-brajesh](https://github.com/not-brajesh)

## License

No licence has been specified here. Add a `LICENSE` file before distributing the project under an open-source licence.
