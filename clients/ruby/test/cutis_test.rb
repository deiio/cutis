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

require File.join(File.dirname(__FILE__), 'test_helper')

class CutisTest < Test::Unit::TestCase
    setup do
        @r = Cutis.new({:port => "6380"})
        # use database 15 for testing so we don't accidentally step
        # on your real data
        @r.select_db(15)
        @r['foo'] = 'bar'
    end

    teardown do
        @r.keys('*').each {|k| @r.delete k}
    end

    context "Commands operating" do
        test "GET" do
            assert_equal 'bar', @r['foo']
        end

        test "SET" do
            @r['foo'] = 'nik'
            assert_equal 'nik', @r['foo']
        end

        test "SETNX" do
            @r['foo'] = 'nik'
            assert_equal 'nik', @r['foo']
            assert_equal true, @r.set_unless_exists('foo', 'bar')
            assert_equal 'nik', @r['foo']
        end

        test "INCR" do
            @r.delete('counter')
            assert_equal 1, @r.incr('counter')
            assert_equal 2, @r.incr('counter')
            assert_equal 3, @r.incr('counter')
        end

        test "DECR" do
            @r['counter'] = '3'
            assert_equal 2, @r.decr('counter')
            assert_equal 1, @r.decr('counter')
            assert_equal 0, @r.decr('counter')
        end

        test "RANDOMKEY" do
            assert_not_nil @r.randkey
        end

        test "RENAME" do
            @r.delete 'foo'
            @r.delete 'bar'
            @r['foo'] = 'hi'
            @r.rename! 'foo', 'bar'
            assert_equal 'hi', @r['bar']
        end

        test "RENAMENX" do
            @r.delete 'foo'
            @r.delete 'bar'
            @r['foo'] = 'hi'
            @r['bar'] = 'ohai'
            assert_raises(CutisError) { @r.rename 'foo', 'bar'}
            assert_equal 'ohai', @r['bar']
        end

        test "EXISTS" do
            @r['foo'] = 'nik'
            assert_equal true, @r.key?('foo')
            @r.delete 'foo'
            assert_equal false, @r.key?('foo')
        end

        test "KEYS" do
            @r.keys('f*').each do |x|
                @r.delete x
            end
            @r['f'] = 'nik'
            @r['fo'] = 'nak'
            @r['foo'] = 'qux'
            assert_equal ['f', 'fo', 'foo'].sort, @r.keys('f*').sort
        end
    end
end
