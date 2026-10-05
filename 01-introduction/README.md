Based on what I did learned, overall the Trader Facing HFT System will look like this.

In other companies production system, this can be different, but the main point still almost the same.

- **Trader** operates the system using GUI
- **Risk Management System (RMS)** will check the risk & reject Trader's command if RMS detect some risk.
- **Strategy service** will decide what to buy or sell, at what price, when to cancel
- **Exchange connector** will convert the request that system understands into something that can be understand by specific exchanges.

```mermaid
flowchart LR
    Trader((Trader)) -- "Input command<br/>(which strategy,<br/>trading param, etc)" --> GUI

    subgraph Cloud["Cloud based (AWS, GCP, etc)"]
        GUI[GUI] --> LB[Load Balancer]
        LB -- Validation --> Risk[Risk Management<br/>Service]
    end

    LB ---> N1
    LB -- "Find which node" ---> N2
    LB ---> N3

    subgraph BareMetal["Bare Metal (Connected with cable with exchange)"]
        N1["Strategy Node 1<br/>(s1, s2, s3)"] --> EC[Exchange<br/>Connector]
        N2["Strategy Node 2<br/>(s2, s4, s5)"] --> EC
        N3["Strategy Node 3<br/>(s3, s5, s6)"] --> EC
    end

    EC -- "Put / Cancel Order" --> Exchange

    subgraph Exchange
        direction TB
        CME ~~~ ICE ~~~ HKEX ~~~ ETC["etc..."]
    end

    style Cloud fill:none,stroke:#2f9e44,stroke-width:2px
    style BareMetal fill:none,stroke:#1971c2,stroke-width:2px
    style Exchange fill:none,stroke:#e03131,stroke-width:2px
```

*s1 – s6 is strategy that can be executed by Strategy Node X*

