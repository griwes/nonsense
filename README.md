# nonsense

## What?

Non-Obviously Named, Simply Effective, NameSpace Engine is a set of tools and systemd unit files that allow you
to (relatively) easily maintain a set of easy to configure network namespaces on your Linux system.

## Why?

The main motivation for the author came from wanting to be able to be on their company VPN, but also have access
to the local network, and only have the work traffic go through the VPN. The author has toyed with QubesOS for a
time, but the lack of hardware acceleration of browsers turned out to be a deal-breaker. Since the author mostly
only cared about the networking aspect of Qubes, network namespaces proved to be a "good enough" replacement for
the more complex system.

## How?

There's several components that make all this work:

  * A daemon, `nonsensed`, that controls the current and saved configurations, and provides additional monitoring
  of service states over what systemd supports natively, and also provides the systemd service units with
  additional dynamic configuration, like subnetting or selecting what namespace is an upstream of the current one.
  * A controller program, `nonsensectl`, that allows you to manipulate the running and saved state of nonsense,
  including changing, on the fly, what network or namespace is considered an upstream of a given namespace, or
  what network addresses it should use.
  * Several systemd unit templates, that serve as the meat-and-potato of the system; controlling their state and
  configuration allows for the system to function.

## Is it functional?

Only as a proof of concept for now. This project is in active development.

## Should I run this?

Probably not yet, but I'm just a README, not your parent.

## But if I want to, what do I need...

### ...to build it?

Build requirements are as follows:

  * CMake 3.13 or higher;
  * a C++20 compiler with coroutines support (as of Dec '19, this means Clang 5 or higher, with libc++);
  * libsystemd, version 241 or higher.

### ...to run it?

The runtime requirements are as follows:

  * Linux kernel, with network namespace support (duh);
  * systemd (duh), version 243 or higher;
  * iproute2;
  * dhclient (for nonsense-managed interfaces configured to use DHCP to obtain their addresses);
  * iw (for nonsense-managed wireless interfaces);
  * bind (for router network namespaces);
  * nftables (for router network namespaces.

### ...to not do to be able to run it?

In addition to the requirements above, the following configurations are **not** supported:

  * using `systemd-resolved` as the DNS service (see
  [this email](https://lists.freedesktop.org/archives/systemd-devel/2017-May/038934.html) as rationale).

## Does this even have tests?

Yes!

The testing "framework" ("environment" would probably be a better word, but whatever) is based on running scenarios
inside Docker containers inside a well-known environment inside Vagrant. This may seem convoluted, but there _is_ a
reason to this madness: some of the scenarios are going to want to test Nonsense taking over wireless interfaces,
and the only reasonable way to do this is by involving the kernel module called `mac80211_hwsim`, which means
inserting and removing it with potentially changing options between scenarious. Additionally, the scenarios will
want to create, pass in, and destroy `veth` interfaces to test non-wireless interface features. All of this could
be potentially harmful to whatever the user's (actual) host environment is.

The currently preferred Vagrant provider is VirtualBox. Please refer to outside guides for installing Vagrant and
additional commands for VM management using it.

All the commands listed below shall be executed in the `test` subdirectory of this project.

To prepare a virtual machine to run the tests on, run:

```
vagrant up
```

To run the full test suite on all supported configurations, run:

```
# Copy the current copy of the Nonsense tree into the testing VM.
# Run this whenever you make changes.
vagrant rsync

# Invoke the test suite. Try also with `-h` appended to this command to see available options.
vagrant ssh -- /nonsense/test/scripts/run_test_suite
```

To tear down the testing VM, run:

```
vagrant destroy
```

