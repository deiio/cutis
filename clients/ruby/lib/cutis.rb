# Ruby Client For Cutis.
# Copyright (c) 2023 furzoom.com, All rights reserved.
# Author: mn, mn@furzoom.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

require 'socket'
require 'timeout'

class CutisError < StandardError
end

class Cutis
    OK = "+OK".freeze
    ERROR = "-".freeze
    NIL = "nil".freeze

    def initialize(opts = {})
        @opts = {:host => 'localhost', :port => '6380'}.merge(opts)
    end

    # SET <key> <val>
    def []=(key, val)
        write "SET #{key} #{val.size}\r\n#{val}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # SETNX <key> <value>
    def set_unless_exists(key, val)
        write "SETNX #{key} #{val.size}\r\n#{val}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # GET <key>
    def [](key)
        write "GET #{key}\r\n"
        res = read_proto
        if res != NIL
            val = read(res.to_i)
            nibble_end
            val
        else
            nil
        end
    end

    # INCR <key>
    def incr(key)
        write "INCR #{key}\r\n"
        read_proto.to_i
    end

    # ICNRBY <key> <num>
    def incrby(key, num)
        write "INCRBY #{key} #{num}\r\n"
        read_proto.to_i
    end

    # DECR <key>
    def decr(key)
        write "DECR #{key}\r\n"
        read_proto.to_i
    end

    # DECRBY <key> <num>
    def decrby(key, num)
        write "DECRBY #{key} #{num}"
        read_proto.to_i
    end

    # RANDOMKEY
    def randkey
        write "RANDOMKEY\r\n"
        read_proto
    end

    # RENAME <oldkey> <newkey>
    def rename!(oldkey, newkey)
        write "RENAME #{oldkey} #{newkey}\r\n"
        res = read_proto
        if res == OK
            newkey
        else
            raise CutisError, res.inspect
        end
    end

    # RENAMENX <oldkey> <newkey>
    def rename(oldkey, newkey)
        write "RENAMENX #{oldkey} #{newkey}\r\n"
        res = read_proto
        if res == OK
            newkey
        else
            raise CutisError, res.inspect
        end
    end

    # EXISTS <key>
    def key?(key)
        write "EXISTS #{key}\r\n"
        read_proto.to_i == 1
    end

    # DEL <key>
    def delete(key)
        write "DEL #{key}\r\n"
        if read_proto == OK
            true
        else
            raise CutisError
        end
    end

    # KEYS <pattern>
    def keys(glob)
        write "KEYS #{glob}\r\n"
        res = read_proto
        if res
            keys = read(res.to_i).split(" ")
            nibble_end
            keys
        end
    end

    # TYPE <key>
    def type?(key)
        write "TYPE #{key}\r\n"
        read_proto
    end

    # RPUSH <key> <string>
    def push_head(key, string)
        write "RPUSH #{key} #{string.size}\r\n#{string}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # LPUSH <key> <string>
    def push_tail(key, string)
        write "RPUSH #{key} #{string.size}\r\n#{string}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # LLEN <key>
    def list_length(key)
        write "LLEN #{key}\r\n"
        read_proto.to_i
    end

    # LRANGE <key> <start> <end>
    def list_range(key, start, ending)
        write "LRANGE #{key} #{start} #{ending}\r\n"
        res = read_proto
        if res[0] == ERROR
            raise CutisError, read_proto
        else
            items = res.to_i
            list = []
            items.times do
                list << read(read_proto.to_i)
                nibble_end
            end
            list
        end
    end

    # LTRIM <key> <start> <end>
    def list_trim(key, start, ending)
        write "LTRIM #{key} #{start} #{ending}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # LINDEX <key> <index>
    def list_index(key, index)
        write "LINDEX #{key} #{index}\r\n"
        res = read_proto
        if res[0] == ERROR
            raise CutisError, read_proto
        elsif res != NIL
            val = read(res.to_i)
            nibble_end
            val
        else
            nil
        end
    end

    # LPOP <key>
    def list_pop_head(key)
        write "LPOP #{key}\r\n"
        res = read_proto
        if res[0] == ERROR
            raise CutisError, read_proto
        elsif res != NIL
            val = read(res.to_i)
            nibble_end
            val
        else
            nil
        end
    end

    # RPOP <key>
    def list_pop_tail(key)
        write "RPOP #{key}\r\n"
        res = read_proto
        if res[0] == ERROR
            raise CutisError, read_proto
        elsif res != NIL
            val = read(res.to_i)
            nibble_end
            val
        else
            nil
        end
    end

    # SELECT <index>
    def select_db(index)
        write "SELECT #{index}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # MOVE <key> <index>
    def move(key, index)
        write "MOVE #{key} #{index}\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # SAVE
    def save
        write "SAVE\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # BGSAVE
    def bgsave
        write "BGSAVE\r\n"
        res = read_proto
        if res == OK
            true
        else
            raise CutisError, res.inspect
        end
    end

    # LASTSAVE
    def lastsave
        write "LASTSAVE\r\n"
        read_proto
    end

    # SHUTDOWN
    def shutdown
        write "SHUTDOWN\r\n"
        read_proto
    end

    # QUIT
    def quit
        write "QUIT\r\n"
        read_proto
    end

    private

    def close
        socket.close unless socket.closed?
    end

    def timeout_retry(time, retries, &block)
        timeout(time, &block)
    rescue TimeoutError
        retries -= 1
        retry unless retries < 0
    end

    def socket
        connect if (!@socket or @socket.closed?)
        @socket
    end

    def connect
        @socket = TCPSocket.new(@opts[:host], @opts[:port])
        @socket.sync = true
        @socket
    end

    def read(length)
        retries = 3
        socket.read(length)
    rescue
        retires -= 1
        if retries > 0
            connect
            retry
        end
    end

    def write(data)
        retries = 3
        socket.write(data)
    rescue
        retries -= 1
        if retries > 0
            connect
            retry
        end
    end

    def nibble_end
        read(2)
    end

    def read_proto
        buff = ""
        while (char = read(1))
            buff << char
            break if buff[-2..-1] == "\r\n"
        end
        buff[0..-3]
    end
end
