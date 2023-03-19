#pragma once

#include <string>

class TokenString
{
public:
   explicit TokenString(const std::string& str = "", char separator = ',');
   virtual ~TokenString();

   void add(const std::string& token);
   void addEmpty();

   bool hasNext() const;

   std::string getNextToken();

   const std::string& getString() const;

   void reset();

private:
   std::string _str;
   char _separator;
   size_t _currentIndex;
};
