# Cutis

## What's Cutis

Cutis is a database. To be more specific Cutis is a very simple database
implementing a dictionary where keys are associated with values. For
example, one can set the key "author" to the string "Niz".

Cutis takes the whole dataset in memory, but the dataset is persistent
since from time to time Cutis writes a dump of the dataset on disk. The
dump is loaded every time the server is restarted.

This means that it can happen that after a system crash the last 
modifications of the dataset are lost, but it's the price to pay for a 
lot of speed. Cutis is the right database for all the applications where
it is acceptable after a crash that some modifications get lost, but where
speed is very important.

However, one can configure Cutis to save the dataset after a given number
of modifications and/or after a given amount of time since the last change
in the dataset. Saving happens in background so the DB will continue to 
serve queries while it is saving the DB dump on disk.

## Does Cutis Support Locking?

No, the idea is to provide atomic primitives in order to make the programmer
able to use Cutis with locking free algorithms. For example imagine one have
10 computers and one Cutis server. One want to count words in a very large
text. This large text is split among the 10 computers, every computer will
process its part and use Cutis's `INCR` command to atomically increment a 
counter for every occurrence of the word found.

INCR/DECR are not the only atomic primitives, there are others like 
`PUSH`/`POP` on lists, `POP RANDOM KEY` operations, `UPDATE` and so on. 
For example, one can use Cutis like a Tuple Space in order to implement 
distributed algorithms.

### Multiple Databases Support

Another synchronization primitive is the support for multiple DBs. By
default, DB 0 is selected for every new connection, but using the `SELECT`
command it is possible to select a different database. The `MOVE` operation
can move an item from one DB to another atomically. This can be used as
a base for locking free algorithms together with other commands.

## Cutis Data Types

Cutis supports the following three data types as values:

- Strings: just any sequence of bytes. Cutis strings are binary safe so
    they can not just hold text, but images, compressed data and everything
    else.
- Lists: lists of strings, with support for operations like append a new
    string on head, on tail, list length, obtain a range of elements, 
    truncate the list to a given length, sort the list, and so on.
- Sets: an unsorted set of strings. It is possible to add or delete elements
    from a set, to perform set intersection, union, subtraction, and so on.

Values can be strings, Lists or Sets. Keys can be a subset of strings not
containing newline (`\n`) and spaces (` `).

Note that sometimes strings may hold numeric values that must be parsed by
Cutis. An example is the `INCR` command that atomically increments the number
stored at the specified key. In this case Cutis is able to handle integers
that can be stored inside a 64-bit signed integer.

### Implementation Details

- Strings are implemented as dynamically allocated strings of characters.
- Lists are implemented as doubly linked lists with cached length.
- Sets are implemented using hash tables that use chaining to resolve 
    collisions.

## Cutis Tutorial

Later in this document you can find detailed information about Cutis 
commands, the protocol specification, and so on. This kind of documentation
is useful but... if you are new to Cutis it is also BORING! The Cutis
protocol is designed so that is both pretty efficient to be parsed by 
computers, but simple enough to be used by humans just poking around
with the 'telnet' command, so this section will show to the reader how
to play a bit with Cutis to get an initial feeling about it, and how it
works.

To start just compile Cutis with `make` and start it with `./cutis-server`.
The server will start and log stuff on the standard output, if you want
it to log more, edit 'conf/cutis.conf', set the loglevel to debug, and
restart it.

You can specify a configuration file as unique parameter:

```bash
./src/cutis-server ./conf/cutis.conf
```

This is NOT required. The server will start event without a configuration
file using a default built-in configuration.

Now let's try to set a key to a given value:

```bash
$ telnet localhost 6380
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
SET foo 3
bar
+OK
```

The first line we sent to the sever is `SET foo 3`. This means "set the key
foo with the following three bytes I'll send you". The following line is
the "bar" string, that is, the three bytes. So the effect is to set the
key "foo" to the value "bar". Very simple!

(Note that you can send commands in lowercase, and it will work anyway,
commands are not case-sensitive)

Note that after the first and the second line we sent to the server there
is a newline at the end. The server expects commands terminated by "\r\n"
and sequence of bytes terminated by "\r\n". This is a minimal overhead 
from the point of view of both the server and client but allow us to play
with Cutis with the telnet command easily.

The last line of the chat between server and client is "+OK". This means
our key was added without problems. Actually `SET` can never fail but
the "+OK" sent lets us know that the server received everything and
the command was actually executed.

Let's try to get the key content now:

```bash
GET foo
3
bar
```

Ok, that's very similar to `SET`, just the other way around. We sent "get
foo", the server replied with a first line that is just a number of bytes
the value stored at the key contained, followed by the actual bytes. Again
"\r\n" are appended both to the bytes count and the actual data.

What about requesting a non-exist key?

```bash
GET blabla
nil
```

When the key does not exist instead of the length just the "nil" string is
sent. Another way to check if a given key exists or not is indeed the 
`EXISTS` command:

```bash
EXISTS foo
1
EXISTS blabla
0
```

As you can see the server replied '1' the first time since 'foo' exists,
and '0' for 'blabla', a key that does not exist.

OK... now you know the basics, read the "Cutis Commands Reference" section
to learn all the commands supported by Cutis and the "Protocol 
Specification" section for more details about the protocol used if you
plan to implement one for a language missing a decent client implementation.

## Cutis Commands Reference

### Connection Handling

- `QUIT`
  - Ask the server to silently close the connection.

### Commands Operating On String Values

- `SET <key> <value>`
  - Time complexity: O(1)
  - Set the string \<value\> as value of the key.
  - The string can't be longer than 1073741824 bytes (1 GB).
- `GET <key>`
  - Time complexity: O(1)
  - Get the value of the specified key. If the key does not exist the 
    special value 'nil' is returned. If the value stored at \<key\> is
    not a string, an error is returned because `GET` can only handle
    string values.
- `SETNX <key> <value>`
  - Time complexity: O(1)
  - `SETNX` works exactly like `SET` with the only difference that if
    the key already exists, no operation is performed. `SETNX` actually
    means "SET if Not eXist".
- `INCR <key>`
- `INCRBY <key> <value>`
  - Time complexity: O(1)
  - Increment the number stored at \<key\> by one. If the key does not
    exist, set the key to the value of "1" (like if the previous value
    was zero). If the value at \<key\> is not a string value an error
    is returned.
  - `INCRBY` works just like `INCR` but instead to increment by 1 the
    increment is \<value\>.
- `DECR <key>`
- `DECRBY <key> <value>`
  - Time complexity: O(1)
  - Decrement the number stored at \<key\> by one. If the key does not
    exist, set the key to the value of "-1" (like if the previous value
    was zero). If the value at \<key\> is not a string value an error
    is returned.
  - `DECRBY` works just like `DECR` but instead to decrement by 1 the
    decrement is \<value\>.

### Commands Operating On Every Value

- `EXISTS <key>`
  - Time complexity: O(1)
  - Test if the specified key exists. The command returns "0" if the key
    exists, otherwise "1" is returned. Note that even keys set with an
    empty string as value will return "1".
- `DEL <key>`
  - Time complexity: O(1)
  - Remove the specified key. If the key does not exist, no operation is
    performed. The command always returns success.
- `TYPE <key>`
  - Time complexity: O(1)
  - Return the type of the value stored at \<key\> in form of a string.
    The type can be one of "NONE", "STRING", "LIST", "SET". NONE is 
    returned if the key does not exist.
- `KEYS <pattern>`
  - Time complexity: O(n) (with n being the number of keys in the DB)
  - Returns all the keys matching the glob-style \<pattern\> as space
    separated strings. For example, if you have in the database the 
    keys "foo" and "foobar" the command `KEYS foo*` will return
    "foo foobar".
  - Note that while the time complexity for this operation is O(n)
    the constant times are pretty low.
- `RANDOMKEY`
  - Time complexity: O(1)
  - Returns a random key from the currently selected DB.
- `RENAME <oldkey> <newkey>`
  - Time complexity: O(1)
  - Atomically renames the key \<oldkey\> to \<newkey\>. If the source
    and destination name are the same, an error is returned. If \<newkey\>
    already exists, it is overwritten.
- `RENAMENX <oldkey> <newkey>`
  - Time complexity: O(1)
  - Just like `RENAME` but fails if the destination key \<newkey\> already
    exists.

### Commands Operating On Lists

- `RPUSH <key> <string>`
  - Time complexity: O(1)
  - Add the given string to the head of the list contained at key. If the
    key does not exist, an empty list is created just before the append
    operation. If the key exists but is not a list, and error is returned.
- `LPUSH <key> <string>`
  - Time complexity: O(1)
  - Add the given string to the tail of the list contained at key. If the
    key does not exist, an empty list is created just before the append
    operation. If the key exists but is not a list, and error is returned.
- `LLEN <key>`
  - Time complexity: O(1)
  - Return the length of the list stored at the specified key. If the key
    does not exist, zero is returned (the same behaviour as for empty
    lists). If the value stored at key is not a list, an error is returned.
- `LRANGE <key> <start> <end>`
  - Time complexity: O(n) (with n being the length of the range)
  - Return the specified elements of the list stored at the specified
    key. Start and end are zero-based indexes. 0 is the first element
    of the list (the list head), 1 the next element and so on.
  - For example `LRANGE foobar 0 2` will return the first three elements
    of the list.
  - \<start\> and \<end\> can also be negative numbers indicating offsets
    from the end of the list. For example, -1 is the last element of the
    list, -2 the penultimate element and so on.
  - Indexes out of range will not produce an error: if start is over the
    end the of list, or start > end, an empty list is returned. If end
    over the end of the list, Cutis will treat it just like the last 
    element of the list.
- `LTRIM <key> <start> <end>`
  - Time complexity: O(n) (with n being len of list - len of range)
  - Trim an existing list so that it will contain only the specified
    range of elements specified. Start and end are zero-based indexes.
    0 is the first element of the list (the list head), 1 the next
    element and so on.
  - For example `LTRIM foobar 0 2` will modify the list stored at "foobar"
    key so that only the first three elements of the list will remain.
  - \<start\> and \<end\> can also be negative numbers indicating offsets
    from the end of the list. For example, -1 is the last element of the
    list, -2 the penultimate element and so on.
  - Indexes out of range will not produce an error: if start is over the
    end the of list, or start > end, an empty list is returned. If end
    over the end of the list, Cutis will treat it just like the last
    element of the list.
  - Hint: the obvious use of `LTRIM` is together with `LPUSH`/`RPUSH`.
    For example:

```
LPUSH mylist <some element>
LTRIM mylist 0 99
```

  - The above two commands will push elements in the list taking care that
    the list will not grow without limits. This is very useful when using
    Cutis to store logs for example. It is important to note that when used
    in this way `LTRIM` is an O(1) operation because in the average case
    just one element is removed from the tail of the list.
- `LINDEX <key> <index>`
  - Time complexity: O(n) (with n being the length of the list)
  - Return the specified elements of the list stored at the specified key.
    0 is the first element, 1 the second and so on. Negative indexes are
    supported, for example -1 is the last element, -2 the penultimate and
    so on.
  - If the value stored at key is not of list type, an error is returned.
    If the index is out of range an empty string is returned.
  - Note that event if the average time complexity is O(n), asking for the
    first or the last element of the list is O(1).
- `LPOP <key>`
  - Time complexity: O(1)
  - Atomically return and remove the first element of the list. For example
    if the list contains the elements "a", "b", "c", `LPOP` will return "a"
    and the list will become "b", "c".
  - If the \<key\> does not exist or the list is already empty, the special
    value "nil" is returned.
- `RPOP <key>`
  - Time complexity: O(1)
  - This commands works exactly like `LPOP`, but the last element instead
    of the first element of the list is returned/deleted.

### Commands Operating On Sets

Work in progress.

### Multiple DB Commands

- `SELELCT <index>`
  - Time complexity: O(1)
  - Select the DB with having the specified zero-based numeric index. For
    default every new client connection is automatically selected to DB 0.
- `MOVE <key> <index>`
  - Time complexity: O(1)
  - Move the specified key from the currently selected DB to the specified
    destination DB. If a key with the same name exists in the destination
    DB, an error is returned.

### Persistence Control Commands

- `SAVE`
  - Save the DB on disk. The server hangs while the saving is not completed,
    no connection is served in the meanwhile. An OK code is returned
    when the DB was fully stored in disk.
- `BGSAVE`
  - Save the DB in background. The OK code is immediately returned. Cutis
    forks, the parent continues to serve the clients, the child saves the
    DB on disk then exit. A client may be able to check if the operation
    succeeded using the `LASTSAVE` command.
- `LASTSAVE`
  - Return the UNIX TIME of the last DB save executed with success. A
    client may check if a `BGSAVE` command succeeded reading the `LASTSAVE`
    value, then issuing a `BGSAVE` command and checking at regular
    intervals every N seconds if `LASTSAVE` changed.
- `SHUTDOWN`
  - Stop all the clients, save the DB, then quit the server. This commands
    makes sure that the DB is switched off without the lost of any data.
    This is not guaranteed if the client uses simply `SAVE` and then `QUIT`
    because other clients may alter the DB data between the two commands.

## Protocol Specification

The Cutis protocol is a compromise between being easy to parse by a 
computer and being easy to parse by a human. Before reading this section
you are strongly encouraged to read the "Cutis Tutorial" section of
this README.md in order to get a first feeling of the protocol playing
with it by TELNET.

### Networking Layer

A client connects to a Cutis server creating a TCP connection to the 
port 6380. Every Cutis command or data transmitted by the client and the
server are terminated by "\r\n" (CRLF).

### Simple INLINE Commands

The simplest commands are the inline commands. This is an example of a
server/client chat (the server chat starts with S:, the client chat with C:)

```
C: PING
S: +PONG
```

An inline command is a CRLF-terminated string sent to the client. The 
server usually replies to inline commands with a single line that can
be a number or a return code.

When the server replies with a return code, if the first character of 
the reply is a "+" then the command succeeded, if it is a "-" then the
following part of the string is an error.

The following is another example of an INLINE command returning an 
integer:

```
C: EXISTS somekey
S: 0
```

Since 'somekey' does not exist, the server returned '0'.

Note that, the `EXISTS` command takes one argument. Argument are 
separated simply by spaces.

### Bulk Commands

A bulk command is exactly like an inline command, but the last argument
of the command must be a stream of bytes in order to send data to the
server. The `SET` command is a bulk command, see the following example:

```
C: SET mykey 6
C: foobar
S: +OK
```

The last argument of the command is '6'. This specifies the number of
DATA bytes that will follow (note that even these bytes are terminated by
two additional bytes of CRLF).

All the bulk commands are in this exact form: instead of the last argument
the number of bytes that will follow is specified, followed by the bytes,
and CRLF. In order to be more clear for the programmer this is the string
sent by the client in the above sample:

```
SET mykey 6\r\nfoobar\r\n
```

### Bulk Replies

The server may reply to an inline or bulk command with a bulk reply. See
the following example:

```
C: GET mykey
S: 6
S: foobar
```

A bulk reply is very similar to the last argument of a bulk command. The
server sends as the first line the number of bytes of the actual reply
followed by CRLF, then the bytes are sent followed by additional two
bytes for the final CRLF. The exact sequence sent by the server is:

``` 
6\r\nfoobar\r\n
```

If the requested value does not exist, the bulk reply will use the special
value 'nil' instead to send the line containing the number of bytes
to read. 

This is an example:

``` 
C: GET nonexistingkey
S: nil
```

The client library API should not return an empty string, but a nil 
object. For example, a Ruby library should return 'nil' while a C
library should return NULL.

### Bulk Reply Error Reporting

Bulk replies can signal errors, for example, trying to use `GET`
against a list value is not permitted. Bulk replies use a negative
bytes count in order to signal an error. An error string of 
ABS(bytes_count) bytes will follow. See the following example:

``` 
C: GET alistkey
S: -42
S: GET against key not holding a string value
```

-42 means: sorry your operation resulted in an error, but a 42 bytes
string that explains this error will follow. Client APIs should abort
on this kind of errors, for example a PHP client should call the die()
function.

### Multi-Bulk Replies

In the specific case of the `LRANGE` command, the server needs to return
multiple values (every element of the list is a value, and `LRANGE` needs
to return more than a single element). This is accomplished using
multiple bulk writes, prefixed by an initial line indicating how many
bulk writes will follow. Example:

``` 
C: LRANGE mylist 0 3
S: 4
S: 3
S: foo
S: 3
S: bar
S: 5
S: Hello
S: 5
S: World
```

The first line the server sent is "4\r\n" in order to specify that four
bulk write will follow. Then every bulk write is transmitted.

If the specified key does not exist instead of the number of elements
in the list, the special value 'nil' is sent. Example:

``` 
C: LRANGE nokey 0 1
S: nil
```

A client library API should return a nil object and not an empty list
when this happens.

### Multi-Bulk Replies Errors

Like bulk reply errors multi-bulk reply errors are reported using a
negative count. Example:

``` 
C: LRANGE stringkey 0 1
S: -43
S: LRANGE against key not holding a list value
```

Check the Bulk replies errors section for more information.

### Multiple Commands And Pipelining

A client can use the same connection in order to issue multiple commands.
Pipelining is supported so multiple commands can be sent with a single
write operation by the client, it is not needed to read the server reply
in order to issue the next command. All the replies can be read at the end.

Usually Cutis server and client will have a very fast link so this is not
very important to support this feature in a client implementation, still
if an application needs to issue a very large number of commands in short
time to use pipelining can be much faster.

## License

Cutis is released under the LGPL license, version 3. See the LICENSE file
for more information.

## Credits

Cutis is written and maintained by Niz.






































































































