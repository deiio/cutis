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

$:.push File.join(File.dirname(__FILE__), '..', 'lib')

require 'rubygems'
require 'cutis'
require 'test/unit'

class Test::Unit::TestCase
    def self.setup(&block)
        define_method :setup do
            super(&block)
            instance_eval(&block)
        end
    end

    def self.teardown(&block)
        define_method :teardown do
            instance_eval(&block)
            super(&block)
        end
    end

    def self.context(*name, &block)
        subclass = Class.new(self)
        remove_tests(subclass)
        subclass.class_eval(&block) if block_given?
        const_set(context_name(name.join(' ')), subclass)
    end

    def self.test(name, &block)
        define_method(test_name(name), &block)
    end


private
    def self.context_name(name)
        "Test#{sanitize_name(name).gsub(/(^| )(\w)/) {$2.upcase}}".to_sym
    end

    def self.test_name(name)
        "test_#{sanitize_name(name).gsub(/\s+/, '_')}".to_sym
    end

    def self.sanitize_name(name)
        name.gsub(/\W+/, ' ').strip
    end

    def self.remove_tests(subclass)
        subclass.public_instance_methods.grep(/^test_/).each do |meth|
            subclass.send(:undef_method, meth.to_sym)
        end
    end
end
