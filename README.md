[![Build Status](https://magnum.travis-ci.com/zeroc-ice/ice-dev.svg?token=icxd1yE9Nf6WLivZz2vF&branch=master)](https://magnum.travis-ci.com/zeroc-ice/ice-dev)

# The Internet Communications Engine (Ice)

Ice is a comprehensive RPC framework that helps you build distributed
applications with minimal effort. Ice takes care of all interactions with
low-level network programming interfaces and allows you to focus your efforts
on your application logic. When using Ice, there is no need to worry about
details such as opening network connections, serializing and deserializing
data for network transmission, or retrying failed connection attempts (to
name just a few of dozens of such low-level details).

Feature highlights:
- Efficient, high-performance binary protocol
- Supports a wide range of programming languages and platforms
- Easy to use and type-safe API, with your own interfaces and types defined in
  a programming language neutral IDL, Slice
- Supports secure, encrypted communications by taking advantage of your
  platform's native SSL/TLS stack
- Familiar object-oriented programming model, with the ability to transmit
  proxies (references to remote objects) to remote applications
- Supports synchronous and asynchronous calls, for both client-side invocations
  and server-side dispatches
- Automatic discovery of remote objects through UDP multicast
- Comes with a number of reusable (and optional) services, including:
   - IceGrid, the nerve-center of any large-scale application, with support for
     server deployment, replication, monitoring, load-balancing and more
   - IceStorm, a lightweight topic-based pub-sub service
   - Freeze, an embedded, transactional database for Ice datatypes
   - Glacier2, a sysadmin-friendly solution for routing Ice communications
     through firewalls

## Copyright and License

Ice is a single-copyright project: all the source code in this ice repository
is Copyright &copy; ZeroC, Inc., with very few exceptions.

As copyright owner, ZeroC can license Ice under different license terms, and
offers the following licenses for Ice:
- GPL v2, a popular open-source license with strong [copyleft](http://en.wikipedia.org/wiki/Copyleft) conditions
  (the default license)
- Commercial or closed-source licenses

If you license Ice under GPL v2, there is no license fee or signed license
agreement: you just need to comply with the GPL v2 terms and conditions. See
[ICE_LICENSE](./ICE_LICENSE) and [LICENSE](./LICENSE) for further information.

If you purchase a commercial or closed-source license for Ice, you must comply
with the terms and conditions listed in the associated license agreement; the
GPL v2 terms and conditions do not apply.

The Ice software itself remains the same: the only difference between an
open-source Ice and a commercial Ice are the license terms.

## Documentation

- [Ice Release Notes](https://doc.zeroc.com/display/Rel/Ice+3.6.0+Release+Notes)
- [Ice Manual](https://doc.zeroc.com/display/Ice36/Home)
