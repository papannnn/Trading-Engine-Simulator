# Low Latency Async Logger

Logger is essentials in every kind of system, it really help the developer to see what's happening when the application is running.

```
09:30:00.000184512 INFO  [md-feed ] Connected to feed session=MD01 seq_start=1048200
09:30:00.000231007 INFO  [gateway ] Order session logged in session=OE01 next_seq=5001
09:30:00.104882113 DEBUG [md-feed ] Tick AAPL bid=189.42x300 ask=189.43x500 seq=1048231
09:30:00.104882990 DEBUG [strategy] Signal BUY AAPL px=189.43 qty=100 reason=imbalance(0.62)
09:30:00.104883410 DEBUG [risk    ] Risk OK AAPL pos=0->100 notional=18943 limit=500000
09:30:00.104884102 INFO  [gateway ] NEW   id=7001 BUY AAPL 100@189.43 IOC tick_to_trade=1989ns
09:30:00.104931557 INFO  [gateway ] ACK   id=7001 exch_id=X88412 rtt=47us
09:30:00.104933020 INFO  [gateway ] FILL  id=7001 100@189.43 pos=100
09:30:00.212449871 WARN  [md-feed ] Seq gap MSFT expected=1048302 got=1048305, requesting snapshot
09:30:00.213002410 INFO  [md-feed ] Snapshot applied MSFT, book rebuilt seq=1048305
09:30:00.318774105 WARN  [risk    ] Blocked TSLA SELL qty=500: max position 300 exceeded
09:30:00.401220876 ERROR [gateway ] REJECT id=7004 reason="price outside limit band"
09:30:01.000000000 INFO  [stats   ] 1s: ticks=184211 orders=37 fills=29 t2t p50=1.9us p99=6.4us max=41.2us
09:30:01.000001200 WARN  [logger  ] Queue full on thread strategy, dropped 12 messages
```

Without logger, we don't know what our system has been going through, maybe our system processed something that shouldn't be processed at all, maybe our system got an error, etc.

## How we want to use this logger

### Fast & Asynchronous

We are working on Low Latency System, that means every microsecond is important for us, having a slow logger will kills our entire performance in systems, we don't want this to happens.

### Handle the log level

Usually there's 4 types of log level in logger

#### Debug

This is useful while developing and debugging

Example log for this

```
DEBUG [strategy] Signal BUY AAPL px=189.43 qty=100 reason=imbalance(0.62)
```

It's just shows why the strategy wants to buy AAPL stock at that point of time. But this is too frequent and too detailed when this goes to production environment.

#### INFO

Normal stuff that useful to know if we record this

Example log for this

```
INFO  [gateway ] FILL id=7001 100@189.43 pos=100
```

This log tells the reader that trade happened, that means money is moving, something that related to money is always worth to log.

#### WARN

Something unexpected happened, but didn't make the system goes down.

Example log for this

```
WARN  [logger  ] Queue full on thread strategy, dropped 12 messages
```

When queue is full, that's when developer need start to jump in and debug what's wrong with the system, it will make the system behave not correctly, but it will not make our system goes down or the impact of it can be ignored.

#### ERROR

It's way worse than WARN, if this happens, please fix your system immediately.

Example log for this

```
ERROR [md-feed ] Snapshot timed out for MSFT, symbol halted
```

When snapshot timed out for specific stock, that means the Orderbook for our system is not updated correctly, making the decision making of Strategy system has a high chance of false positive or making the Strategy system got shutdown for some specific stock, the impact is huge, we need to fix this.

## Interface

Based on the requirement that we have, we concluded the interface will look like this. Although we're gonna "break" this interface for performance reason, but for now, this is good enough for simple logger.

```c++
#include <string>

enum class Level { Debug, Info, Warn, Error };

class Logger {
public:
    void log(Level level, const std::string& message);

    void debug(const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);
};
```

We use the logger like this

```c++
Logger logger;
logger.info(std::format("Order {} filled: {} @ {:.2f}", 7001, 100, 189.43));
```

## Design #1

For the first design, we can do this one.

```c++
#include <iostream>

void Logger::log(Level level, const std::string& message) {
    switch (level) {
        case Level::Debug: std::cout << "DEBUG "; break;
        case Level::Info:  std::cout << "INFO ";  break;
        case Level::Warn:  std::cout << "WARN ";  break;
        case Level::Error: std::cout << "ERROR "; break;
    }
    std::cout << message << '\n';
}

void Logger::debug(const std::string& message) { log(Level::Debug, message); }
void Logger::info(const std::string& message)  { log(Level::Info,  message); }
void Logger::warn(const std::string& message)  { log(Level::Warn,  message); }
void Logger::error(const std::string& message) { log(Level::Error, message); }
```

But for this design, there's some flaws we need to know.

### System call increase the latency

Under the hood, message that we passed to `cout` will eventually passed into `write` system call.

We don't want system call happening in our hot path, because it will slow down the hot path's thread performance

### String formatting in hot path increase the latency

Converting

```c++
std::format("Order {} filled: {} @ {:.2f}", 7001, 100, 189.43)
```

Into this string

```c++
Order 7001 filled: 100 @ 189.43
```

Is not cheap, but still acceptable in non hot path, we will fix this in the last design because this requires "breaking" the interface.

### Using long string introduces heap allocation

`string` actually lives either in `stack` memory or `heap` memory, depending how long the `string`, if the `string` is short, the implementation will using `Small String Optimization (SSO)`.

Under the hood, it's basically using `stack` to store the `string` value. If the `string` is long, `SSO` can't be used at all.

We don't want to trigger heap allocation, heap allocation eventually will trigger system call, system call will increase the latency of our system, especially in hot path.

## Design #2

Instead of logging the message in the same thread, we can delegate the `cout` calling on another thread.

```c++
class Logger {
public:
    void log(Level level, const std::string& message) {
        worker_.push({level, message});
    }

    void debug(const std::string& message) { log(Level::Debug, message); }
    void info(const std::string& message)  { log(Level::Info,  message); }
    void warn(const std::string& message)  { log(Level::Warn,  message); }
    void error(const std::string& message) { log(Level::Error, message); }

private:
    LogWorker worker_;
};
```

```c++
class LogWorker {
public:
    LogWorker() : thread_([this] { run(); }) {}

    ~LogWorker() {
        stop_ = true;
        thread_.join();
    }

    void push(const LogEntry& entry) {
        if (!buffer_.try_push(entry)) {
            ++dropped_;
        }
    }

private:
    void run() {
        LogEntry entry;
        while (!stop_) {
            while (buffer_.try_pop(entry)) {
                print(entry);
            }
        }
        while (buffer_.try_pop(entry)) {   // write out anything pushed before stop
            print(entry);
        }
    }

    static void print(const LogEntry& entry) {
        switch (entry.level) {
            case Level::Debug: std::cout << "DEBUG "; break;
            case Level::Info:  std::cout << "INFO ";  break;
            case Level::Warn:  std::cout << "WARN ";  break;
            case Level::Error: std::cout << "ERROR "; break;
        }
        std::cout << entry.message << '\n';
    }

    RingBuffer<LogEntry, 1024> buffer_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> dropped_{0};
    std::thread thread_;
};
```

```mermaid
flowchart TB
    subgraph TT["Trading thread (hot path)"]
        A["Your code<br/>logger.info(msg)"] --> B["Logger<br/>only inserts"]
        B --> C["LogWorker<br/>push(entry)"]
    end

    C --> RB["RingBuffer<br/>try_push / try_pop"]
    RB -. full .-> D["Dropped<br/>if buffer is full"]

    subgraph WT["Worker thread (background)"]
        E["run()<br/>pops entries"] --> F["cout<br/>system call here"]
    end

    RB --> E
```

That means, after hot path calls

```c++
logger.info(std::format("Order {} filled: {} @ {:.2f}", 7001, 100, 189.43));
```

It's just inserting to the queue on `LogWorker`, and `LogWorker` eventually will execute that message and call `cout`.

The data structure that we use for the queue is called `RingBuffer`, `RingBuffer` basically is a lock free data structure that can be used as a queue.

More detailed explanation is on another topic.

For the non hot path, this design actually already good enough. But this still not solving the string formatting problem and heap allocation on string.

## Design #3

It time to "break" our interface a little bit for improving performance. Instead of only accepting string, we also accepting struct object.

The reason we use struct object because we will delegate the string creation on another worker (Formatter Worker), and then after Formatter Worker created the string, string will be passed into LogWorker to be logged.

And struct object creation are cheap, we basically creating the value on the stack, which is cheap and fast.

```mermaid
flowchart TB
    subgraph TT["Trading thread (hot path)"]
        A["Your code<br/>logger.info(event)"] --> B["Logger<br/>picks log()"]
        B --> C["FormatWorker<br/>push: copy bytes"]
    end

    C --> RBF["Ring buffer formatter<br/>level, fn, bytes"]

    subgraph FT["Format thread"]
        D["FormatWorker<br/>run: pop entry"] --> E["to_string()<br/>bytes to string"]
        E --> F["LogWorker<br/>push(string)"]
    end

    RBF --> D
    F --> RBO["Ring buffer output<br/>level, string"]

    subgraph OT["Output thread"]
        G["LogWorker<br/>run: pop entry"] --> H["cout<br/>print the line"]
    end

    RBO --> G
```

So we start from making the struct that already know how to convert it into string

```c++
struct OrderFilled {
    int id;
    int qty;
    double price;

    std::string to_string() const {
        return std::format("Order {} filled: {} @ {:.2f}", id, qty, price);
    }
};

struct Text {                       // plain strings are wrapped in this
    char value[120];

    Text() = default;

    explicit Text(std::string_view message) {
        std::size_t n = std::min(message.size(), sizeof(value) - 1);
        std::memcpy(value, message.data(), n);
        value[n] = '\0';
    }

    std::string to_string() const { return value; }
};

template <typename T>
concept Loggable = requires(const T& event) { event.to_string(); };
```

Then we pass the struct / string into Logger object

```c++
class Logger {
public:
    void log(Level level, std::string_view message) { formatter_.push(level, Text(message)); }

    template <Loggable T>
    void log(Level level, const T& event) { formatter_.push(level, event); }

    template <typename T> void debug(const T& value) { log(Level::Debug, value); }
    template <typename T> void info(const T& value)  { log(Level::Info,  value); }
    template <typename T> void warn(const T& value)  { log(Level::Warn,  value); }
    template <typename T> void error(const T& value) { log(Level::Error, value); }

private:
    FormatWorker formatter_;
};
```

We use logger object like this

```c++
Logger logger;

OrderFilled fill{7001, 100, 189.43};
logger.debug(fill);

// Or in one line
logger.debug(OrderFilled{7001, 100, 189.43});
```

After we called `debug` from `Logger` object, `Logger` object will delegate the formatting to `FormatWorker` queue.

```c++
class FormatWorker {
public:
    explicit FormatWorker(LogWorker& output) : output_(output), thread_([this] { run(); }) {}

    ~FormatWorker() {
        stop_ = true;
        thread_.join();
    }

    // Trading thread: copy bytes only, no formatting, no allocation
    template <Loggable T>
    void push(Level level, const T& event) {
        static_assert(std::is_trivially_copyable_v<T>, "struct must be safe to memcpy");
        static_assert(sizeof(T) <= sizeof(Entry::data), "struct too big for one slot");

        Entry entry;
        entry.level = level;
        entry.to_string = [](const char* data) {
            T event;
            std::memcpy(&event, data, sizeof(T));
            return event.to_string();
        };
        std::memcpy(entry.data, &event, sizeof(T));

        if (!ring_buffer_formatter_.try_push(entry)) ++dropped_;
    }

private:
    struct Entry {
        Level level;
        std::string (*to_string)(const char* data);
        char data[128];
    };

    // Format thread: turn bytes into a string, hand it to LogWorker
    void run() {
        Entry entry;
        while (!stop_) {
            while (ring_buffer_formatter_.try_pop(entry)) {
                output_.push({entry.level, entry.to_string(entry.data)});
            }
        }
        while (ring_buffer_formatter_.try_pop(entry)) {   // anything pushed before stop
            output_.push({entry.level, entry.to_string(entry.data)});
        }
    }

    LogWorker& output_;
    RingBuffer<Entry, 1024> ring_buffer_formatter_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> dropped_{0};
    std::thread thread_;
};
```

Basically what it does is to convert the struct that we pass, into string that can be read by `LogWorker`.

After the formatting complete, the string will be passed into `LogWorker` queue.

```c++
class LogWorker {
public:
    LogWorker() : thread_([this] { run(); }) {}

    ~LogWorker() {
        stop_ = true;
        thread_.join();
    }

    // Format thread: hand over a finished string
    void push(const LogEntry& entry) {
        if (!ring_buffer_output_.try_push(entry)) ++dropped_;
    }

private:
    // Output thread: the only place that touches cout
    void run() {
        LogEntry entry;
        while (!stop_) {
            while (ring_buffer_output_.try_pop(entry)) {
                print(entry);
            }
        }
        while (ring_buffer_output_.try_pop(entry)) {      // anything pushed before stop
            print(entry);
        }
    }

    static void print(const LogEntry& entry) {
        switch (entry.level) {
            case Level::Debug: std::cout << "DEBUG "; break;
            case Level::Info:  std::cout << "INFO ";  break;
            case Level::Warn:  std::cout << "WARN ";  break;
            case Level::Error: std::cout << "ERROR "; break;
        }
        std::cout << entry.message << '\n';
    }

    RingBuffer<LogEntry, 1024> ring_buffer_output_;
    std::atomic<bool> stop_{false};
    std::atomic<std::size_t> dropped_{0};
    std::thread thread_;
};
```

Compared to Design #1, this Logger is much better.

## Handling minimum level of logger

In production environment, we don't want to enable `DEBUG` level because it's too verbose. But we want to change the minimum level on the fly when we need it.

For example, when we need to `DEBUG` the production environment behavior when something is wrong.

```mermaid
flowchart TB
    subgraph CT["Config thread (every second)"]
        DB["Config DB<br/>level = DEBUG"] --> W["watch_config()<br/>read level"]
        W --> S["set_level()<br/>store new level"]
    end

    S --> L["min_level_<br/>std::atomic#lt;Level#gt;"]

    subgraph TT["Trading thread (hot path)"]
        A["Your code<br/>logger.debug(...)"] --> B["Logger<br/>level #lt; min?"]
        B -- passes --> C["FormatWorker<br/>push: copy bytes"]
    end

    L -. read .-> B
    B -. below minimum .-> X["Skipped<br/>returns right away"]
    C --> R["Format + output threads<br/>format, then print"]
```

We add 1 more class to fetch the config every `X` seconds.

```c++
class ConfigWatcher {
public:
    explicit ConfigWatcher(Logger& logger) : logger_(logger), thread_([this] { run(); }) {}

    ~ConfigWatcher() {
        stop_ = true;
        thread_.join();
    }

private:
    void run() {
        while (!stop_) {
            logger_.set_level(read_level_from_config());   // database, file, HTTP...
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    Logger& logger_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};
```

We tweak the logger class a little bit to add minimum level validation.

```c++
class Logger {
public:
    void set_level(Level level) {
        min_level_.store(level, std::memory_order_relaxed);
    }

    void log(Level level, std::string_view message) {
        if (is_skipped(level)) [[likely]] return;
        formatter_.push(level, Text(message));
    }

    template <Loggable T>
    void log(Level level, const T& event) {
        if (is_skipped(level)) [[likely]] return;
        formatter_.push(level, event);
    }

    template <typename T> void debug(const T& value) { log(Level::Debug, value); }
    template <typename T> void info(const T& value)  { log(Level::Info,  value); }
    template <typename T> void warn(const T& value)  { log(Level::Warn,  value); }
    template <typename T> void error(const T& value) { log(Level::Error, value); }

private:
    bool is_skipped(Level level) const {
        return level < min_level_.load(std::memory_order_relaxed);
    }

    std::atomic<Level> min_level_{Level::Info};
    FormatWorker formatter_;
};
```

Noticed I added `[[likely]]` there?

It's because in production environment, we usually only enable minimum log level `INFO`, and because `DEBUG` level usually a lot, a lot of `DEBUG` level log will get dropped. We use `[[likely]]` to give a hint to compiler that this line of code will `likely` got executed, helping to minimize the `Control Hazard`.

## Closing

Thanks everyone for reading this, if you want to give some feedback or just want to say hi, can contact me on [Linkedin](https://www.linkedin.com/in/taufanmaaruf/).

Next topic, I will discuss Memory Pool