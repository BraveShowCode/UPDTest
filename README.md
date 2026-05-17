# UDP Multicast ZeroMQ Discovery

This repository contains a small C++17 UDP multicast discovery example. A client multicasts a `DISCOVER_ZMQ_CONFIG` request, and any server that receives it replies directly to the client's source address and port with ZeroMQ connection settings by UDP unicast.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Configuration

Server settings live in `config/server.conf`:

- `multicast_group` / `multicast_port`: multicast channel used for discovery requests.
- `bind_address`: local UDP address the server binds to; `0.0.0.0` listens on all IPv4 interfaces.
- `interface`: IPv4 interface address used when joining the multicast group; `0.0.0.0` lets the OS choose.
- `poll_timeout_ms`: receive-loop poll interval used to handle shutdown signals.
- `zeromq_endpoint`, `zeromq_pub_endpoint`, `zeromq_sub_endpoint`, `zeromq_topic`: values returned in discovery responses.

Client settings live in `config/client.conf`:

- `multicast_group` / `multicast_port`: multicast channel to send the request to.
- `bind_address`: local UDP address the client binds to.
- `interface`: IPv4 interface address used for outbound multicast; `0.0.0.0` lets the OS choose.
- `listen_port`: UDP port that receives the unicast response; `0` uses an ephemeral source port.
- `timeout_ms`: maximum time to wait for a server response.

## Run

Start the server in one terminal:

```bash
./build/udp_multicast_server config/server.conf
```

Run the client in another terminal or host on the same multicast-capable network:

```bash
./build/udp_multicast_client config/client.conf
```

Example response:

```text
Received unicast response from 10.0.0.10:30001
ZMQ_CONFIG v1 request_id=... endpoint=tcp://10.0.0.10:5555 pub_endpoint=tcp://10.0.0.10:5556 sub_endpoint=tcp://10.0.0.10:5557 topic=market-data
```

## Protocol

1. Client sends `DISCOVER_ZMQ_CONFIG v1 request_id=<id>` to the configured multicast group and port.
2. Server validates the request prefix and builds a `ZMQ_CONFIG v1` response.
3. Server sends that response with `sendto` to the client's source IP and UDP source port, so the response is unicast rather than multicast.
