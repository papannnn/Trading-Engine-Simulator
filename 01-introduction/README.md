Based on what I did learned, overall the Trader Facing HFT System will look like this.

I learned all of this knowledge from doing a personal discussion with someone, shoutout to [Karan Lodhi Rajput](https://www.linkedin.com/in/karan-lodhi-rajput-b1a44b105/) for sharing this knowledge to me. 

He also have a youtube channel that talks about HFT stuff. If you interested, you can also visit his [youtube channel](https://www.youtube.com/channel/UCl2vVwtbL0CIeB8YKK6q4ig)

## Introduction

In other companies production system, this can be different, but the main point still almost the same.

- **Trader** operates the system using GUI
- **Risk Management System (RMS)** will check the risk & reject Trader's command / Strategy execution if RMS detect some risk.
- **Strategy service** will decide what to buy or sell, at what price, when to cancel automatically based on the data it knows (Current price, how many stock we own, floating profit, etc).
- **Exchange connector** will convert the request that system understands into something that can be understand by specific exchanges.
- **Order Management System** tells you have many stocks you have, running order status, floating profit / loss.
- **Market Data Feed Handler** listening the exchange activity and updating it's own orderbook.

```mermaid
flowchart LR
    Trader((Trader)) --> GUI

    subgraph Control["Control plane (office or cloud)"]
        GUI[Trader GUI]
        Router[Command Router]
        OMS["Order Management Service<br/>(positions, P&L)"]
        GUI -- "Commands" --> Router
        OMS -- "Positions, fills,<br/>floating P&L" --> GUI
    end

    subgraph Colo["Colocation (bare metal next to exchange)"]
        MDFH["Market Data<br/>Feed Handler"]
        Strategy["Strategy Nodes<br/>(each strategy pinned to one node)"]
        Risk["Risk Management<br/>Service"]
        EC[Exchange Connector]
        MDFH -- "Local order book" --> Strategy
        Strategy -- "New / Cancel / Amend" --> Risk
        Risk -- "Approved orders" --> EC
        EC -- "Acks, fills, rejects" --> Strategy
    end

    Router -- "Start / stop,<br/>set params" --> Strategy
    Router -- "Limits,<br/>kill switch" --> Risk

    EX["Exchange<br/>CME / ICE / HKEX / etc."]
    EC -- "Order entry (TCP)<br/>FIX or binary" --> EX
    EX -- "Execution reports" --> EC
    EX -- "Market data<br/>(UDP multicast)" --> MDFH
    EC -- "Execution reports" --> OMS
    EX -. "Drop copy" .-> OMS
    MDFH -. "Prices for P&L" .-> OMS

    style Control fill:none,stroke:#2f9e44,stroke-width:2px
    style Colo fill:none,stroke:#1971c2,stroke-width:2px
    style EX fill:none,stroke:#e03131,stroke-width:2px,color:#e03131
```

*s1 – s6 is strategy that can be executed by Strategy Node X*

## Exchange Connector

The reason we require Exchange Connector because each exchange might having different way of communicating, we called this **protocol**.

This is just an example

For example, Chicago Mercantile Exchange (CME) might need to communicate using Binary Protocol in order to send order to that exchange.

Or maybe Intercontinental Exchange (ICE) might want to communicate using FIX Protocol.

```mermaid
flowchart LR
    subgraph Example[" "]
        direction LR
        CME --> Binary[Binary Protocol]
        ICE --> FIX[FIX Protocol]
    end

    classDef red fill:none,stroke:#e03131,stroke-width:2px
    class CME,ICE,Binary,FIX red
    style Example fill:none,stroke:#e03131,stroke-width:2px
```

So **Exchange Connector** is not only for forwarding the request from the **Trader** to the **Exchange**, but also acts as an adapter for the request object to a form that can any **Exchange** can understand.

## Market Data Feed Handler

**Market Data Feed Handler** acts as a listener for every market activity on the **Exchange**.

**Exchange** will sends a raw packets, each of packets contains >= 1 message.

Here's a simplified example how packets looks like:

```
Packet #1001
  Sent at   : 09:30:00.000123456
  Messages  : 4

  [1] New order   order 5003   ABC   Buy    500 @ 100.50
  [2] Modify      order 5001   ABC   qty 300 -> 200   (price 100.40)
  [3] Cancel      order 4998   ABC
  [4] Trade       ABC   100 @ 100.60   (sell order 5002 partly filled)
```

This will means:

1. Someone created an order to buy 500 unit `ABC` stock at $100.50 with ID 5003
2. Someone modify an order from 300 to 200 unit for `ABC` stock at price level $100.40 with ID 5001
3. Someone canceled the order for buying stock `ABC` with ID 4998
4. Trade happened for stock `ABC`, sold 100 unit, but partly filled with ID 5002.

With these information, we can create the orderbook that looks like this (Simplified).

```
          Buy           |           Sell
   Qty       Price      |      Price       Qty
   800      100.50      |     100.60       200
   600      100.40      |     100.70       400
   900      100.30      |     100.80       700
```

And then, based on the data from Orderbook, Strategy service now can decide what to do on `ABC` stock (Buy, Sell, Modify, Cancel, etc).

You may wondering, why we need sequence ID for each packets?

**Exchange** sends packets using UDP protocol, UDP is fast, but unreliable, it's using fire and forget mechanism.

```
   Exchange                        Feed Handler
      |                          (last seq = 100)
      |                                  |
      |------ packet #101 -------X       |   lost on the network
      |                                  |
      |------ packet #102 -------------->|   expected #101, got #102
      |                                  |   -> gap! #101 is missing
      |                                  |   -> hold #102, pause strategy
      |                                  |
      |<----- resend #101 ---------------|
      |                                  |
      |------ packet #101 -------------->|   apply #101, then #102
      |                                  |   last seq = 102, book OK
```

Assuming we're already in sequence number `100`, we expect to process packet number `101` in next, because we're using UDP, there's a chance packet number `101` got lost, then comes packet number `102`, but we haven't processed packet number `101`. 

There's must be something wrong, so when packet got lost, we can ask to **Exchange** again to retransmit packet no `101`.

```mermaid
flowchart TB
    EX[Exchange] -- "UDP multicast" --> NIC["Network Interface Card<br/>(NIC)"]
    NIC -- "Raw packets" --> MDFH["Market Data Feed Handler<br/><br/>1. Sequence check<br/>2. Decode messages<br/>3. Book update"]
    MDFH -- "New order / Cancel /<br/>Modify / Trade" --> OB[Orderbook]
    OB -- "Notify orderbook changes<br/>(callback / shared memory)" --> S[Strategy]
    MDFH -. "Gap detected:<br/>request retransmit / snapshot" .-> EX
```

You may wondering, why **Exchange** sends market activity using UDP, why not using TCP?.

### TCP is one to one connection

TCP protocol is one to one connection, if there's X number of receiver, that means **Exchange** need to have X number of connections opened.

```mermaid
flowchart LR
    EX[Exchange] --> Us[Our HFT Company]
    EX --> C1["HFT Company<br/>Competitor #1"]
    EX --> C2["HFT Company<br/>Competitor #2"]
    EX --> C3["HFT Company<br/>Competitor #3"]
    EX --> C4["HFT Company<br/>Competitor #..."]
```

Imagine if there's thousands of HFT company want to listen to the **Exchange**, that means **Exchange** need to have thousands of TCP Connections running.

Not only that, if we want to send `Y` number packets, we need to multiply by `Z` number of connections.

```c++
for (int i = 0 ; i < connections.size(); i++) {
    sendMessage(packet, connections[i]);
}
```

Imagine we want to send 1000 packets, for 1000 HFT companies. That means we're gonna send 1 mil times, that's inefficient.

### Unecessary TCP acks

TCP protocol is reliable because it make sure the receiver get the message. One of the mechanism to make sure receiver get the message by acks mechanism.

That means, if **Exchange** is sending 1000 message to 1000 HFT Companies. Not only **Exchange** need to send message 1 mil times, it also will get acks from each HFT Companies, more accurately, it will get 1 mil of acks. That's a lot of acks to be processed.

## UDP Protocol

Now let's talk about UDP Protocol, UDP Protocol has 3 types.

### Unicast

Unicast is one on one communication, like TCP does.

### Broadcast

Broadcast will send to all receiver, even though some receiver doesn't interested to listen to some kind of packets.

```mermaid
---
title: Broadcast
---
flowchart LR
    EX[Exchange] -- "Send all packets<br/>to everyone" --> All

    subgraph All["Everyone on the same network"]
        Us[Our HFT Company]
        C1["HFT Company<br/>Competitor #1"]
        C2["HFT Company<br/>Competitor #2"]
        C3["HFT Company<br/>Competitor #3"]
        C4["HFT Company<br/>Competitor #..."]
    end

    style All fill:none,stroke:#868e96,stroke-width:2px
```

### Multicast

Multicast is different, **Exchange** only sends the packets to the specific group that interested to that specific packets.

```mermaid
---
title: Multicast
---
flowchart LR
    EX[Exchange] -- "Send packets<br/>stock related only" --> Stock
    EX -- "Send packets<br/>futures related only" --> Futures

    subgraph Stock["Multicast Group Stock"]
        Us[Our HFT Company]
        C1["HFT Company<br/>Competitor #1"]
    end

    subgraph Futures["Multicast Group Futures"]
        C2["HFT Company<br/>Competitor #2"]
        C3["HFT Company<br/>Competitor #3"]
        C4["HFT Company<br/>Competitor #..."]
    end

    style Stock fill:none,stroke:#2f9e44,stroke-width:2px,color:#2f9e44
    style Futures fill:none,stroke:#e03131,stroke-width:2px,color:#e03131
```