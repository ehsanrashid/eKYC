# eKYC Engine

A high-performance, asynchronous engine for eKYC (electronic Know Your Customer) identity verification, built on top of [Aeron](https://github.com/aeron-io/aeron) for low-latency messaging, PostgreSQL for identity data storage, and SBE (Simple Binary Encoding) for efficient message serialization.

---

## Features

- **Aeron UDP Messaging:** Subscribes and publishes to Aeron UDP channels for fast, reliable message transport.
- **PostgreSQL Integration:** Connects to PostgreSQL database for identity verification against stored records.
- **SBE Message Processing:** Uses Simple Binary Encoding for efficient serialization/deserialization of identity messages.
- **Identity Verification Workflow:** Processes "Identity Verification Request" messages and validates against database records.
- **Asynchronous Processing:** Uses background threads for message polling and processing.
- **Custom Logging:** Integrates a custom logger for file-based, level-controlled logging with fast logging capabilities.
- **Database Connection Management:** Maintains persistent PostgreSQL connections with error handling.

---

## Requirements

- **C++17** or newer
- **CMake** 3.16 or newer
- **PostgreSQL** development libraries (`libpqxx`)
- **Aeron** (fetched and built automatically)
- **Custom Logger Library**:
  - Header: `/usr/local/include/logger.h`
  - Library: `/usr/local/lib/libloggerlib.a`
  - **Install logger from:** [https://github.com/Huzaifa309/loggerLib](https://github.com/Huzaifa309/loggerLib)
- **Custom Wrappers**:
  - `libaeronWrapper.a` (provided in `lib/`)
  - `libpgWrapper.a` (provided in `lib/`)

---

## Database Setup

1. **Install PostgreSQL:**
   ```sh
   sudo apt-get install postgresql postgresql-contrib libpqxx-dev
   ```

2. **Create database and user:**
   ```sql
   -- As postgres user
   CREATE DATABASE ekycdb;
   CREATE USER huzaifa WITH PASSWORD '3214';
   GRANT ALL PRIVILEGES ON DATABASE ekycdb TO huzaifa;
   ```

3. **Create users table:**
   ```sql
   -- Connect to ekycdb
   \c ekycdb
   
   CREATE TABLE users (
       id SERIAL PRIMARY KEY,
       type VARCHAR(20),
       identity_number VARCHAR(20) NOT NULL,
       name VARCHAR(100) NOT NULL,
       date_of_issue DATE,
       date_of_expiry DATE,
       address TEXT,
       logged_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
   );
   
   -- Insert sample data
   INSERT INTO users (type, identity_number, name, date_of_issue, date_of_expiry, address)
   VALUES ('cnic', '4210109729681', 'Huzaifa Ahmed', '2020-01-15', '2030-01-15', 'Gulshan-e-Iqbal, Block 2');
   ```

---

## Build Instructions

1. **Clone the repository** and enter the project directory:
   ```sh
   git clone https://github.com/ehsanrashid/eKYC
   cd eKYC
   ```

2. **Install dependencies:**
   ```sh
   # Install PostgreSQL development libraries
   sudo apt-get install libpqxx-dev
   
   # Install logger library
   # Follow instructions at https://github.com/Huzaifa309/loggerLib
   ```

3. **Generate SBE messages** (if schema changes):
   ```sh
   java -jar sbe-all-1.36.0-SNAPSHOT.jar login-schema.xml
   ```

4. **Build the project using CMake:**
   ```sh
   mkdir build
   cd build
   cmake ..
   make
   ```

---

## Usage

1. **Ensure PostgreSQL is running:**
   ```sh
   sudo systemctl start postgresql
   ```

2. **Run the executable:**
   ```sh
   ./eKYC
   ```

3. The engine will:
   - Connect to PostgreSQL database (`ekycdb`)
   - Connect to Aeron Media Driver
   - Start background message processing
   - Begin listening for identity verification requests

4. **To stop the engine:** Use Ctrl+C or stop the process.

---

## Message Processing Workflow

1. **Receive Message:** Engine receives SBE-encoded `IdentityMessage` on Aeron subscription channel
2. **Decode Message:** Extracts identity information (name, ID, type, etc.)
3. **Check Message Type:** Processes only "Identity Verification Request" messages with `verified=false`
4. **Database Verification:** Queries PostgreSQL to verify identity against stored records
5. **Log Results:** Logs verification success/failure
6. **Response:** (TODO) Send back verification result message

### Sample Identity Message Fields:
- `msg`: "Identity Verification Request"
- `type`: "cnic" or "passport"
- `id`: Identity number (e.g., "4210109729681")
- `name`: Full name (e.g., "Huzaifa Ahmed")
- `dateOfIssue`: Issue date
- `dateOfExpiry`: Expiry date
- `address`: Address information
- `verified`: "true" or "false"

---

## Configuration

- **Database Connection:**
  - Host: `localhost`
  - Port: `5432`
  - Database: `ekycdb`
  - User: `huzaifa`
  - Password: `3214`

- **Aeron Channels:**
  - Subscription: `aeron:udp?endpoint=0.0.0.0:50000`, Stream ID: `1001`
  - Publication: `aeron:udp?endpoint=anas.eagri.com:40124`, Stream ID: `100`

- **Log Files:**
  - Format: `Gateway_SBE_<timestamp>.log`
  - Location: `build/logs/`
  - Max size: 10 MB

---

## Project Structure

```
.
├── CMakeLists.txt
├── eKYCEngine.cpp          # Main engine implementation
├── eKYCEngine.h            # Engine class definition
├── main.cpp                # Application entry point
├── helper.h                # Helper functions (string_to_bool)
├── login-schema.xml        # SBE schema definition
├── sbe-all-1.36.0-SNAPSHOT.jar  # SBE code generator
├── include/
│   ├── aeron_wrapper.h     # Aeron messaging wrapper
│   └── pg_wrapper.h        # PostgreSQL wrapper
├── lib/
│   ├── libaeronWrapper.a   # Aeron wrapper library
│   └── libpgWrapper.a      # PostgreSQL wrapper library
├── output/
│   └── my_app_messages/    # Generated SBE message classes
│       ├── IdentityMessage.h
│       ├── MessageHeader.h
│       └── Char64str.h
└── build/
    └── logs/               # Log output directory
```

---

## Development

### Adding New Message Types
1. Update `login-schema.xml` with new message schema
2. Regenerate SBE classes: `java -jar sbe-all-1.36.0-SNAPSHOT.jar login-schema.xml`
3. Update message processing logic in `verify_and_respond()`

### Database Schema Changes
- Modify table structure as needed
- Update SQL queries in `verify_identity()` method
- Test with sample data

---

## Troubleshooting

- **Database Connection Failed:**
  - Verify PostgreSQL is running: `sudo systemctl status postgresql`
  - Check database credentials and permissions
  - Ensure `ekycdb` database exists

- **SBE Compilation Errors:**
  - Verify namespace includes: `my::app::messages::`
  - Check generated message classes in `output/my_app_messages/`

- **Aeron Connection Issues:**
  - Ensure Aeron Media Driver is accessible
  - Check network connectivity for UDP endpoints

- **Logger Not Found:**
  - Install logger library from [https://github.com/Huzaifa309/loggerLib](https://github.com/Huzaifa309/loggerLib)
  - Verify files exist: `/usr/local/include/logger.h` and `/usr/local/lib/libloggerlib.a`

---
