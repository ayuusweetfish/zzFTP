# Project report: zzFTP, a miniature FTP suite

zzFTP is a miniature FTP server/client suite that handles a basic subset
of the File Transfer Protocol as has been described in
[RFC 959](http://www.ietf.org/rfc/rfc959.txt).

The supported FTP commands are as follows, with extensions to the
assignment requirement marked in bold:
- QUIT
- SYST (Returns fixed string)
- TYPE (ASCII only)
- USER
- PASS (supports anonymous log-in and hard-coded authentication)
- PORT
- PASV
- CWD
- PWD
- MKD
- RMD
- RNFR
- RNTO
- **DELE**
- LIST
- **REST**
- RETR
- STOR
- ABOR

To build the server, run `make` under the `server/` directory and refer
to its help output by `./server -help`. The client's documentation
resides in its own folder `client/`.

The source code and documentation has been uploaded to
[GitHub](https://github.com/kawa-yoiko/zzFTP), dual licensed under the
Hippocratic License and AGPLv3.

An instance has been deployed at zzftp.kawa.moe (66.42.69.75) at port 2111,
and is planned to continue to run until at least 31 Oct 2020.

## Features and highlights

### Bottom line: functional FTP server/client

In addition to the interoperations between the server and the client of the
suite, the server has been tested against the FileZilla client, and the client
has been tested against the vsFTPd server; both are fully functional.

### Bottom line: no resource leaks

The suite has been written carefully with proper resource management in mind.
The server has been tested with `valgrind` with adequate normal usage, and
has exhibited no memory leaks or file descriptor leaks so far.

### The delete (DELE) command

The server supports removing files and empty directories through this command.

### Non-blocking handling of data transfer

zzFTP utilizes multi-threading, using proper locks and signals between
threads to minimize busy looping and ensure correct control flows.

During our testing, vsFTPd exhibited a minor shortcoming in that in passive
mode, it waits for the client to connect to its data connection port before
emitting the 150 mark, hence some implementations of the client will wait
for the 150 mark while vsFTPd waits for the client to connect, resulting in
an unexpected infinite loop. zzFTP is a multi-threaded server and handles
commands sent to the control connection at any time, be it abort (ABOR),
quit (QUIT), or system type query (TYPE); other commands are ignored with a
comment. In this way, a client can abort the transmission after the STOR/RETR
command, without the actual connection being established.

### Recovery of broken transmissions

zzFTP server supports the restart (REST) command with both retrieving and
storing, allowing the client to read/write files at any specified offset,
hence allowing recovery of broken transmissions. However, it should be noted
that the zzFTP client currently does not make use of this feature of the server.

### Security

The zzFTP server does not allow the client to access any files other than
the specified root directory. There is a simple test suite to test the
subroutines handling directory names to ensure no escape could happen through
them. In the future, this can be further strengthened by using `chroot` and
dropping all privileges at program entry. Currently there is a dependency
on the `ls` system utility, rendering this approach impossible, but getting
rid of this dependency should be trivial.
