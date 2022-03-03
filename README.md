# trmc2d: a temperature daemon

This package contains the source code of trmc2d: a temperature
daemon, based on libtrmc2, for driving the TRMC2 temperature
controller. It also has the code of two plugins for converting raw
measurements to temperature: interpolate.so interpolates over
tabulated values, while expression.so accepts a conversion law
written as a symbolic expression.

## Requirements

For building trmc2d, you will need libtrmc2 and its associated header
file Trmc.h installed somewhere your compiler can find them. If they
are installed somewhere gcc won't look, you will have to edit the
Makefile and add the appropriate -I and -L options.

For having readline support in trmc2d's shell mode, you will also
need libreadline and libtermcap, which a quite standard.

For interpolate.so to provide interpolation methods other than
linear, you will need GSL, the GNU Scientific Library, available
from <http://www.gnu.org/software/gsl/>.

For the expression.so plugin, you will need libmatheval, available
from <https://www.gnu.org/software/libmatheval/>.

On a Debian-like system, you could type

```bash
sudo apt install libreadline-dev libgsl-dev libmatheval-dev
```

to get all these dependencies but libtrmc2.

## Compiling

The supplied Makefile assumes you have all of the above
dependencies. If this is not the case, edit the Makefile, look for
the following:

    # Comment-out if libreadline is not available.
    # Doing so will disable line editing facilities in shell mode.
    WITH_READLINE = yes

    # Comment-out if libgsl is not available.
    # Doing so will disable all methods but `linear' from interpolate.so.
    WITH_GSL = yes

    # Comment-out if libmatheval is not available.
    # Doing so will disable building expression.so.
    WITH_MATHEVAL = yes

and comment-out whichever lines you have to.

Then type `make`. You will get trmc2d, interpolate.so and, unless you
disabled it, expression.so.

## Running

Type `trmc2d -d` as root, then connect a client to TCP port 5025.

Or type `trmc2d -s` as root and talk to it at the keyboard.

## Files

* README.md:          this file
* doc/:               documentation directory
  * protocol.html:    the trmc2d language
  * plugins.html:     writing plugins
* Makefile:           needed for `make`
* plugins/:           plugins for converting raw values to temperature
  * Makefile:              for building the plugins
  * interpolate-linear.c:  linear interpolation
  * interpolate.c:         interpolation based on GSL
  * expression.c:          expression evaluation
* \*.c, \*.h:           source code of trmc2d

## Bugs

The program is in beta testing.

Basic temperature measurements should work, as in the following
example session:

    $ sudo ./trmc2d -s
    trmc2d> *idn?
    trmc2d temperature server, Institut NEEL, Nov 2016
    trmc2d> channel:count?
    trmc2d> error?
    _TRMC_NOT_INITIALIZED
    trmc2d> start 50
    trmc2d> channel:count?
    7
    trmc2d> channel2:type?
    6 (_TYPEE)
    trmc2d> channel2:voltage:range 1e-5
    trmc2d> channel2:current:range 8e-7
    trmc2d> channel2:priority 2
    trmc2d> channel2:priority?
    2 (_ALWAYS)
    trmc2d> channel2:measure?
    9.89719
    trmc2d> quit
    $

You can also `telnet localhost 5025` and have the same kind of
dialog with trmc2d.

You can optionally use a Unix domain socket instead of a TCP port
but then, if you kill trmc2d instead of asking it to `quit`, it will
not remove the socket. Next time you try to run trmc2d, it will exit
immediately and you will see no error message... unless you look in
your system's logs:

```text
# grep trmc2d /var/log/messages
Jul 16 22:18:05 bonet trmc2d: bind: Address already in use
```

Just remove the socket and restart trmc2d.

## Author

Edgar Bonet <bonet@grenoble.cnrs.fr>
