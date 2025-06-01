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

require 'benchmark'
require 'optparse'

$:.push File.join(File.dirname(__FILE__), 'lib')
require 'cutis'

count = 20000

options = {:port => "6380", :address => "localhost"}

OptionParser.new do |opts|
    opts.banner = "usage: ruby #{$0} [options]"

    opts.on("-p", "--port port", "cutis port") do |port|
        options[:port] = port
    end

    opts.on("-a", "--address address", "cutis address") do |addr|
        options[:address] = addr
    end

    opts.on("-h", "--help", "show this text") do
        puts opts
        exit
    end
end.parse!

text = "The first line we sent to the server is some text"
@c = Cutis.new(options)
Benchmark.bmbm do |x|
    x.report("set") { count.times {|i| @c["foo#{i}"] = "#{text} #{i}"; @c["foo#{i}"]}}
end
