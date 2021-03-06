
// project headers
#include "my_time.h"
// standard headers
#include <string>
#include <ctime>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cmath>
#include <iomanip>

using namespace std;
using namespace My;

// constructors

// construct Time object using current time
Time::Time() {
  // initialize local-UTC time shift
  initLocalUtcShift();
  // current Unix time
  time_t c_unixtime = time(NULL);
  // fill Time object
  initByUnixtime(c_unixtime);
}

// construct Time object using year, month, day, hour and second
Time::Time(int year, int month, int day, int hour, int minute, double second) {
  // initialize local-UTC time shift
  initLocalUtcShift();
  // initialize and fill it with time data we already know
  tm c_tm = {0};
  c_tm.tm_year = year-1900; // number of years since 1900
  c_tm.tm_mon = month-1; // in range 0..11
  c_tm.tm_mday = day;
  c_tm.tm_hour = hour+_local_utc_shift; // taking timezone into account
  c_tm.tm_min = minute;
  c_tm.tm_sec = int(second);
  // calculate Unix time base on ctime structure
  time_t c_unixtime = mktime(&c_tm);
  // fill Time object
  double tmp;
  initByUnixtime(double(c_unixtime)+modf(second,&tmp));
}

// construct Time object using year, day of year, hour, minute and second
Time::Time(int year, int doy, int hour, int minute, double second) {
  // initialize local-UTC time shift
  initLocalUtcShift();
  // initialize and fill ctime strucure with available time data
  tm c_tm = {0};
  c_tm.tm_year = year-1900;
  c_tm.tm_mon = 0;
  c_tm.tm_mday = 1;
  c_tm.tm_hour = hour+_local_utc_shift; // take into account the timezone
  c_tm.tm_min = minute;
  c_tm.tm_sec = int(second);
  // calculate Unix time
  time_t c_unixtime = mktime(&c_tm)+(doy-1)*86400;
  // fill Time object
  double tmp;
  initByUnixtime(double(c_unixtime)+modf(second,&tmp));
}

// construct Time object using formatted string representation of time
Time::Time(string dateTimeString) {
  // initialize local-UTC time shift
  initLocalUtcShift();
  // replace "-" and ":" with " "
  dateTimeString[4] = ' ';
  dateTimeString[7] = ' ';
  dateTimeString[13] = ' ';
  dateTimeString[16] = ' ';
  // create stream with date string as a source
  istringstream dateTimeStream(dateTimeString);
  // read time data from date string stream
  int year, month, day, hour, minute;
  double second;
  dateTimeStream >> year >> month >> day >> hour >> minute >> second;
  // create ctime structure
  tm c_tm = {0};
  c_tm.tm_year = year-1900;
  c_tm.tm_mon = month-1;
  c_tm.tm_mday = day;
  c_tm.tm_hour = hour+_local_utc_shift;
  c_tm.tm_min = minute;
  c_tm.tm_sec = int(second);
  // calculate Unix time
  time_t c_unixtime = mktime(&c_tm);
  // and finally fill the Time object
  double tmp;
  initByUnixtime(double(c_unixtime)+modf(second,&tmp));
}

// construct Time object using Unix timestamp
Time::Time(double unixtime) {
  // initialize local-UTC time shift
  initLocalUtcShift();
  // initialize Time object by unixtime
  initByUnixtime(unixtime);
}

// other methods

// return formatted time string
string Time::strtime() {
  ostringstream timeStream; // initialize stringstream for time
  // fill up the time string
  timeStream << setw(4) << _year << "-"
             << setw(2) << setfill('0') << _month << "-"
             << setw(2) << setfill('0') << _day << " "
             << setw(2) << setfill('0') << _hour << ":"
             << setw(2) << setfill('0') << _minute << ":"
             << setw(6) << setprecision(3) << fixed << _second;
  // get the string out of the stream
  return timeStream.str();
}

// add "amount" of "type" (year, month, day, hour, minute, second) time to the
// Time object
Time& Time::add(int amount, string type) {
  // create ctime structure and fill it with time data from the Time object
  tm c_tm = {0};
  c_tm.tm_year = _year-1900;
  c_tm.tm_mon = _month-1;
  c_tm.tm_mday = _day;
  c_tm.tm_hour = _hour;
  c_tm.tm_min = _minute;
  c_tm.tm_sec = int(_second);
  // check the type of time data to add and add it
  if (type == "year") {
    c_tm.tm_year += amount;
  } else if (type == "month") {
    c_tm.tm_mon += amount;
  } else if (type == "day") {
    c_tm.tm_mday += amount;
  } else if (type == "hour") {
    c_tm.tm_hour += amount;
  } else if (type == "minute") {
    c_tm.tm_min += amount;
  } else if (type == "second") {
    c_tm.tm_sec += amount;
  } else { // throw an error - unknoown time type
    cout << "Unknown time type" << endl;
  }
  // take into account the local-UTC shift
  c_tm.tm_hour += _local_utc_shift;
  // get the Unix time of the new time
  time_t c_unixtime = mktime(&c_tm);
  // recalculate members of the Time object
  double tmp;
  initByUnixtime(double(c_unixtime)+modf(_second,&tmp));

  return *this; // chained method
}

// private methods

// initialize shift between local and UTC time
void Time::initLocalUtcShift() {
  // current time
  time_t c_time = time(NULL);
  // initialize ctyme structures for local and UTC times
  tm * c_tm_local, * c_tm_utc;
  // evaluate local time structure
  c_tm_local = localtime(&c_time);
  // get Unix time for the local time
  int c_sec_local = mktime(c_tm_local);
  // evaluate UTC time structure
  c_tm_utc = gmtime(&c_time);
  // get Unix time for the UTC time
  int c_sec_utc = mktime(c_tm_utc);
  // calculate and save time shift between local and UTC time in hours
  _local_utc_shift = (c_sec_local-c_sec_utc)/3600;
}

// initialize Timeobject members with Unix time
//void Time::initByUnixtime(time_t c_unixtime) {
void Time::initByUnixtime(double unixtime) {

  _unixtime = unixtime; // save full unixtime
  // integral part of unixtime and millisecond part
  double int_unixtime, millisecond;
  // extract integral unixtime and milliseconds
  millisecond = modf(unixtime, &int_unixtime);
  // initialize integer unixtime from integral part
  time_t c_unixtime = time_t(int_unixtime);

  // ctime structure of the current time
  tm c_tm = *gmtime(const_cast<time_t*>(&c_unixtime));

  // setting Time object values for the current time
  _year = c_tm.tm_year+1900; // exact value of year
  _month = c_tm.tm_mon+1; // in 1..12 range
  _day = c_tm.tm_mday; // in 1..31 range
  _hour = c_tm.tm_hour; // in 0..23 range
  _minute = c_tm.tm_min; // in 0..59 range
  _second = c_tm.tm_sec; // in 0..61 range, leap seconds included
  if (_second > 59) _second = 59; // drop off leap seconds, now 0..59 range
  _doy = c_tm.tm_yday+1; // we store days of year in 1..366 range
  // add milliseconds to seconds
  _second += millisecond;
}

