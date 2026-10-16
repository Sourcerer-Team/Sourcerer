#!/usr/bin/env ruby
# frozen_string_literal: true

# Syntax highlighting test for Ruby: classes, modules, methods, blocks,
# symbols, strings and heredocs, control flow, rescue and pattern matching.

require 'json'
require_relative 'lib/helpers'

=begin
A block comment
spanning several lines.
=end

module Sourcerer
  MAX_RETRIES = 3
  VERSION = '1.0.13'

  class Account < Base
    include Comparable
    attr_accessor :owner, :balance

    def initialize(owner, balance = 0.0, *extras, currency: 'EUR', **options, &block)
      @owner = owner
      @balance = balance
      @@count ||= 0
      @@count += 1
      $last_account = self
    end

    def deposit(amount)
      raise ArgumentError, "amount must be positive" unless amount.positive?
      @balance += amount
      self
    end

    def <=>(other)
      balance <=> other.balance
    end

    def to_s = "#{owner}: #{format('%.2f', balance)}"

    def self.open(*args, **kw)
      new(*args, **kw)
    end

    private

    def audit!
      yield balance if block_given?
    rescue IOError, SystemCallError => e
      warn "audit failed: #{e.message}"
      retry if (attempts ||= 0) < MAX_RETRIES
    ensure
      puts 'audited'
    end
  end
end

# Symbols, hashes, arrays and ranges.
config = { name: 'sourcerer', "port": 8080, :retries => 3, nested: { ok: true } }
symbols = %i[alpha beta] + [:gamma, :'quoted symbol']
words = %w[one two three]
range = (1..10).step(2).to_a
bits = (0x1F & 0b1010) | (7 << 2) ^ ~0
matrix = Array.new(3) { |i| Array.new(3) { |j| i * j } }

# Blocks, lambdas and procs.
doubled = [1, 2, 3].map { |n| n * 2 }
sum = [1, 2, 3].inject(0) do |acc, n|
  acc + n
end
square = ->(x) { x * x }
greet = proc { |name = 'world'| "hello #{name}" }
puts square.(4), greet.call

# Strings, heredocs and regular expressions.
path = "/tmp/#{ENV.fetch('USER', 'unknown')}/file.txt"
raw = 'single #{not_interpolated} \n'
template = <<~TEXT
  Dear #{config[:name]},
    indented line
  Thanks!
TEXT
pattern = %r{\A(\d+)-(\w+)\z}i
match = '42-answer' =~ pattern ? $1.to_i : nil
command = `echo hello`

# Control flow.
case config[:retries]
when 0..2 then puts 'few'
when 3, 4 then puts 'some'
else puts 'many'
end

value = if config[:name] && !config[:name].empty?
          config[:name].upcase
        elsif config[:port] > 1024
          'high port'
        else
          nil
        end

i = 0
while i < 3
  i += 1
  next if i == 2
  break unless i < 3
end
until i.zero? do i -= 1 end
for k in 1..2 do print k end

# Pattern matching and safe navigation.
case { status: 'ok', data: [1, 2] }
in { status: 'ok', data: [first, *rest] } if first.positive?
  puts "first=#{first}"
in Integer | Float => num
  puts num
end

owner = config.dig(:nested, :ok)&.then { |v| !v }
defined?(Rails) && __method__

begin
  JSON.parse('{')
rescue JSON::ParserError => error
  warn error.message
ensure
  puts 'parse attempted'
end

__END__
This text after __END__ is data, not code.
