# eKYC Engine

A high-performance, asynchronous engine for eKYC (electronic Know Your Customer) message processing, built on top of [Aeron](https://github.com/aeron-io/aeron) for low-latency messaging and a custom logger for robust, file-based logging.

---

## Features

- **Aeron UDP Messaging:** Subscribes and publishes to Aeron UDP channels for fast, reliable message transport.
- **Asynchronous Processing:** Uses background threads for message polling and processing.
- **Custom Logging:** Integrates a custom logger (from `/usr/local/include/logger.h` and `/usr/local/lib/libloggerlib.a`) for file-based, level-controlled logging.
- **Graceful Shutdown:** Handles shutdown via user input (press Enter to stop).

---

## Requirements

- **C++17** or newer
- **CMake** 3.16 or newer
- **Aeron** (fetched and built automatically)
- **Custom Logger Library**:
  - Header: `/usr/local/include/logger.h`
  - Library: `/usr/local/lib/libloggerlib.a`
  - **Install logger from:** [https://github.com/Huzaifa309/loggerLib](https://github.com/Huzaifa309/loggerLib)
- **libaeronWrapper.a** (provided in `lib/`)

---

## Build Instructions

1. **Clone the repository** and enter the project directory:
   ```sh
   git clone <your-repo-url>
   cd eKYC
   ```

2. **Install the logger library:**
   - Follow the instructions at [https://github.com/MahmoudAbdelRahman/logger](https://github.com/MahmoudAbdelRahman/logger) to build and install the logger.
   - Ensure the header file is at `/usr/local/include/logger.h` and the static library is at `/usr/local/lib/libloggerlib.a`.

3. **Build the project using CMake:**
   ```sh
   mkdir build
   cd build
   cmake ..
   make
   ```

   This will fetch and build Aeron, and then build the `eKYC` executable.

---

## Usage

1. **Run the executable:**
   ```sh
   ./eKYC
   ```

2. The engine will start, connect to the Aeron Media Driver, and begin processing messages.
3. **To stop the engine:** Press `Enter` in the terminal.

---

## Configuration

- **Aeron Channels and Streams:**  
  These are hardcoded in `eKYCEngine.h`:
  - Subscription: `aeron:udp?endpoint=172.17.10.58:50000`, Stream ID: `1001`
  - Publication: `aeron:udp?endpoint=239.101.9.9:40124`, Stream ID: `100`
- **Log File:**  
  Logs are written to `Gateway_JSON.log` in the working directory, with a max size of 10 MB.

---

## Project Structure

```
.
├── CMakeLists.txt
├── eKYCEngine.cpp
├── eKYCEngine.h
├── include/
│   └── aeron_wrapper.h
├── lib/
│   └── libaeronWrapper.a
├── main.cpp
```

---

## Extending

- **To change log file or size:** Edit the `Logger` initialization in `eKYCEngine.h`/`.cpp`.
- **To change Aeron channels:** Edit the static constants in `eKYCEngine.h`.

---

## Troubleshooting

- **Logger Not Found:**  
  Ensure `/usr/local/include/logger.h` and `/usr/local/lib/libloggerlib.a` exist and are accessible.
- **Aeron C++ API Deprecation Warning:**  
  The project currently uses the Aeron C++ API, which will be removed in Aeron 1.50.0. Plan to migrate to the C++ Wrapper API in the future.

---

## License

Specify your license here. 