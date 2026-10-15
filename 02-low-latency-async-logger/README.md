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
