#!/usr/bin/env lua
-- Syntax highlighting test for Lua: comments, strings, numbers, keywords,
-- functions, tables, metamethods, goto labels and attributes.

--[[ A block comment
     spanning several lines ]]
--[==[ A block comment with level 2 ]==]

local Account = {}
Account.__index = Account

local MAX_BALANCE <const> = 1e6
local hex, bin = 0xFF, 0x1p-4
local name, greeting = "Sourcerer", 'single quoted'
local escaped = "tab\tquote\" newline\n unicode \u{48}"
local raw = [[
raw string with "quotes" and -- not a comment
]]
local leveled = [==[ level two ]==]

-- Constructor with a table of fields, nested tables and a method.
function Account.new(owner, balance)
  local self = setmetatable({}, Account)
  self.owner = owner
  self.balance = balance or 0
  self.history = { { kind = "open", amount = balance }, [1] = true }
  return self
end

function Account:deposit(amount)
  if amount <= 0 then
    error("invalid amount: " .. tostring(amount))
  elseif self.balance + amount > MAX_BALANCE then
    return nil, "limit"
  else
    self.balance = self.balance + amount
  end
  table.insert(self.history, { kind = "deposit", amount = amount })
  return self.balance
end

local function sum(...)
  local total = 0
  for _, value in ipairs({ ... }) do
    total = total + value
  end
  return total
end

-- Operators, precedence and bitwise ops.
local expr = not (hex > 10 and bin ~= 0) or (hex // 16) % 3 == 0
local bits = (hex & 0x0F) | (bin << 2) ~ (~hex >> 1)
local joined = name .. "-" .. greeting .. "..."
local length = #joined + #{ 1, 2, 3 }

-- Loops and goto.
for i = 1, 10, 2 do
  if i % 3 == 0 then goto continue end
  print(i)
  ::continue::
end

for key, value in pairs({ a = 1, b = 2 }) do
  print(key, value)
end

local count = 0
repeat
  count = count + 1
until count >= 3

while count > 0 do
  count = count - 1
end

-- Metamethods and method calls.
Account.__tostring = function(self)
  return string.format("%s: %.2f", self.owner, self.balance)
end

local acct = Account.new("Ada", 100)
acct:deposit(50)
print(acct, sum(1, 2, 3), select("#", nil, false))

local ok, err = pcall(function()
  acct:deposit(-1)
end)

if not ok then
  io.stderr:write("error: ", err, "\n")
end

return Account
