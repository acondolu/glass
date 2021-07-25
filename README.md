# glass
![version-shield]
[![MIT License][license-shield]][license-url]
[![Build status][status-shield]][status-url]

<p align="center">
  <img src="assets/glass-logo.png" width="150"/>
</p>

`glass` allows any two nodes on the internet to establish a TCP connection. Its main features are:

- _Flexibility:_ peers need not have a stable IP address, nor allow any incoming connection. _glass_ is specifically designed to traverse NATs (e.g. home routers).
- _Privacy:_ each peer does not need to disclose its IP address to the other peer. A _glass router_ acts as an intermediary node, relaying the contents of the connection while hiding the identity (and geolocation) of peers.
- _Decentralization:_ the goal is to empower users by facilitating direct TCP connections, thus reducing the centralizing power of omnipotent servers.

## Design

A _glass router_ acts as the _glass_ service provider; its only role is relaying TCP segments from one peer to the other. The data exchanged during the connection is treated as opaque (could and should be end-to-end encrypted) and is not even buffered. The router service includes two parts:
- **Translator**: performs address and port translation on incoming TCP segments, in order to redirect them to the correct peer.
- **Controller**: manages new connection requests by finding suitable open ports, and adding them to the translation table.

A _glass client_ can bind itself to a _channel_ (the glass equivalent of a TCP port) or connect to an existing channel:
- When a client is bound to a channel, it keeps an open control connection with the router. In case of an incoming peer connection request, the router will push the connection request, consisting of an IP address + TCP port. TCP data exchanged with the supplied endpoint will be relayed to the connecting peer.
- When a client needs to connect to an existing channel, it sends a connection request to the router. The router replies with an IP address + TCP port. TCP data exchanged with the supplied endpoint will be relayed to the receiving peer.

## FAQ
1. **Does glass protect peer identity 100%?**
   
   No, a malicious attacker with enough resources (i.e. a surveillance state) could record all incoming and outgoing packets at a glass router, and link two endpoints by simply matching outgoing/incoming packets. This may be mitigated in the future by federating glass routers and using built-in encryption. Nevertheless, the router would still be vulnerable to timing attacks.

## Building

To install dev dependencies:
```bash
sudo apt-get install build-essential libnfnetlink-dev libnetfilter-queue-dev
```

To build the translator:
```bash
make out/glass
```

## Router configuration

The router needs to be supplied a TCP port range, to whom clients will connect in order to reach their peers. Any port range can be chosen, but we advice to only use 49152 - 65535, i.e. the one for unregistered services and ephemeral connections.

First, one can verify the local ephemeral ports range with:

```
cat /proc/sys/net/ipv4/ip_local_port_range
```

Say we want to reserve range 60000 - 65535 for our glass router. 
To ensure that the OS will not bind user sockets to ports in that interval, we should disallow all ports above 59999, like this:

```
echo 32768 59999 > /proc/sys/net/ipv4/ip_local_port_range
```

Then, instruct `iptables` to enqueue all incoming IP packets to chosen TCP ports into, say, queue number 6:
```
iptables -A INPUT -p tcp --dport 60000:65535  -j NFQUEUE --queue-num 6
```

(It is also possible to specify a network interface with `-i`, for instance `-i eth0`.)

## Launching

#### Router
To start the translator:
```bash
sudo glass-translator [QUEUE_NUM] [CONTROL_UNIX_SOCKET] 
```
For instance: `glass 6 /tmp/glass.control.s`.

To start the controller:
```bash
glass-controller [PORT] [CONTROL_UNIX_SOCKET] 
```

#### Client

To expose the local TCP server at port 1234:
```bash
glass-client bind 1234 [ROUTER_ADDRESS]
```

It will print to stdout a fresh channel id.

To connect to a given channel:

```bash
glass-client conn [LOCAL_PORT] [CHANNEL_ID] [ROUTER_ADDRESS]
```

## Known issues
- The glass router does not (yet) support fragmented IP packets. To force the kernel to reassemble fragments, we could enable connection tracking by passing `-m conntrack` to `iptables`. However, note that connection tracking will limit the number of connections to the size of the `conntrack` table (by default, 32768 entries).
- By design, only one active TCP connection is allowed per host IP per destination port on a glass router. When the same IP address needs to open multiple connections, the controller will assign different destination ports.

<!-- LICENSE -->
## License

(c) 2021 Andrea Condoluci. Distributed under the GNU Lesser General Public License v3.0. See `LICENSE` for more information.


<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[license-shield]: https://img.shields.io/badge/license-LGPL--3.0-success
[license-url]: https://github.com/acondolu/glass/blob/main/LICENSE
[status-shield]: https://img.shields.io/github/workflow/status/acondolu/glass/build/master
[status-url]: https://github.com/acondolu/glass/actions
[version-shield]: https://img.shields.io/badge/version-alpha-important