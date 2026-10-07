Based on what I did learned, overall the Trader Facing HFT System will look like this.

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

```mermaid
flowchart LR
    subgraph HFT["HFT System"]
        direction LR
        Strategy -- "Request object" --> EC[Exchange Connector]
    end

    EC -- "Binary Protocol" --> CME
    EC -- "FIX Protocol" --> ICE
    EC -- "JSON" --> HKEX

    classDef red fill:none,stroke:#e03131,stroke-width:2px
    class Strategy,EC,CME,ICE,HKEX red
    style HFT fill:none,stroke:#e03131,stroke-width:2px
```