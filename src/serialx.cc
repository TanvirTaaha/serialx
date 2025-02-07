/**  Copyright 2012 William Woodall and John Harrison
 *
 * Edit: Renamed from "Serial" to "SerialX" to avoid confusion with other libraries
 * Modified by: Tanvir Hossain Taaha <tanvir.taaha@gmail.com>
 */
#include <algorithm>

#if !defined(_WIN32) && !defined(__OpenBSD__) && !defined(__FreeBSD__)
#include <alloca.h>
#endif

#if defined(__MINGW32__)
#define alloca __builtin_alloca
#endif

#include "serialx/serialx.h"

#ifdef _WIN32
#include "serialx/impl/win.h"
#else
#include "serialx/impl/unix.h"
#endif

using std::invalid_argument;
using std::min;
using std::numeric_limits;
using std::size_t;
using std::string;
using std::vector;

using serialx::bytesize_t;
using serialx::flowcontrol_t;
using serialx::IOException;
using serialx::parity_t;
using serialx::SerialX;
using serialx::SerialXException;
using serialx::stopbits_t;

class SerialX::ScopedReadLock {
 public:
  ScopedReadLock(SerialXImpl *pimpl) : pimpl_(pimpl) {
    this->pimpl_->readLock();
  }
  ~ScopedReadLock() {
    this->pimpl_->readUnlock();
  }

 private:
  // Disable copy constructors
  ScopedReadLock(const ScopedReadLock &);
  const ScopedReadLock &operator=(ScopedReadLock);

  SerialXImpl *pimpl_;
};

class SerialX::ScopedWriteLock {
 public:
  ScopedWriteLock(SerialXImpl *pimpl) : pimpl_(pimpl) {
    this->pimpl_->writeLock();
  }
  ~ScopedWriteLock() {
    this->pimpl_->writeUnlock();
  }

 private:
  // Disable copy constructors
  ScopedWriteLock(const ScopedWriteLock &);
  const ScopedWriteLock &operator=(ScopedWriteLock);
  SerialXImpl *pimpl_;
};

SerialX::SerialX(const string &port, uint32_t baudrate, serialx::Timeout timeout,
                 bytesize_t bytesize, parity_t parity, stopbits_t stopbits,
                 flowcontrol_t flowcontrol)
    : pimpl_(new SerialXImpl(port, baudrate, bytesize, parity,
                             stopbits, flowcontrol)) {
  pimpl_->setTimeout(timeout);
}

SerialX::~SerialX() {
  delete pimpl_;
}

void SerialX::open() {
  pimpl_->open();
}

void SerialX::close() {
  pimpl_->close();
}

bool SerialX::isOpen() const {
  return pimpl_->isOpen();
}

size_t
SerialX::available() {
  return pimpl_->available();
}

bool SerialX::waitReadable() {
  serialx::Timeout timeout(pimpl_->getTimeout());
  return pimpl_->waitReadable(timeout.read_timeout_constant);
}

void SerialX::waitByteTimes(size_t count) {
  pimpl_->waitByteTimes(count);
}

size_t
SerialX::read_(uint8_t *buffer, size_t size) {
  return this->pimpl_->read(buffer, size);
}

size_t
SerialX::read(uint8_t *buffer, size_t size) {
  ScopedReadLock lock(this->pimpl_);
  return this->pimpl_->read(buffer, size);
}

size_t
SerialX::read(std::vector<uint8_t> &buffer, size_t size) {
  ScopedReadLock lock(this->pimpl_);
  uint8_t *buffer_ = new uint8_t[size];
  size_t bytes_read = 0;

  try {
    bytes_read = this->pimpl_->read(buffer_, size);
  } catch (const std::exception &e) {
    delete[] buffer_;
    throw;
  }

  buffer.insert(buffer.end(), buffer_, buffer_ + bytes_read);
  delete[] buffer_;
  return bytes_read;
}

size_t
SerialX::read(std::string &buffer, size_t size) {
  ScopedReadLock lock(this->pimpl_);
  uint8_t *buffer_ = new uint8_t[size];
  size_t bytes_read = 0;
  try {
    bytes_read = this->pimpl_->read(buffer_, size);
  } catch (const std::exception &e) {
    delete[] buffer_;
    throw;
  }
  buffer.append(reinterpret_cast<const char *>(buffer_), bytes_read);
  delete[] buffer_;
  return bytes_read;
}

string
SerialX::read(size_t size) {
  std::string buffer;
  this->read(buffer, size);
  return buffer;
}

size_t
SerialX::readline(string &buffer, size_t size, string eol) {
  ScopedReadLock lock(this->pimpl_);
  size_t eol_len = eol.length();
  uint8_t *buffer_ = static_cast<uint8_t *>(alloca(size * sizeof(uint8_t)));
  size_t read_so_far = 0;
  while (true) {
    size_t bytes_read = this->read_(buffer_ + read_so_far, 1);
    read_so_far += bytes_read;
    if (bytes_read == 0) {
      break;  // Timeout occured on reading 1 byte
    }
    if (string(reinterpret_cast<const char *>(buffer_ + read_so_far - eol_len), eol_len) == eol) {
      break;  // EOL found
    }
    if (read_so_far == size) {
      break;  // Reached the maximum read length
    }
  }
  buffer.append(reinterpret_cast<const char *>(buffer_), read_so_far);
  return read_so_far;
}

string
SerialX::readline(size_t size, string eol) {
  std::string buffer;
  this->readline(buffer, size, eol);
  return buffer;
}

vector<string>
SerialX::readlines(size_t size, string eol) {
  ScopedReadLock lock(this->pimpl_);
  std::vector<std::string> lines;
  size_t eol_len = eol.length();
  uint8_t *buffer_ = static_cast<uint8_t *>(alloca(size * sizeof(uint8_t)));
  size_t read_so_far = 0;
  size_t start_of_line = 0;
  while (read_so_far < size) {
    size_t bytes_read = this->read_(buffer_ + read_so_far, 1);
    read_so_far += bytes_read;
    if (bytes_read == 0) {
      if (start_of_line != read_so_far) {
        lines.push_back(
            string(reinterpret_cast<const char *>(buffer_ + start_of_line),
                   read_so_far - start_of_line));
      }
      break;  // Timeout occured on reading 1 byte
    }
    if (string(reinterpret_cast<const char *>(buffer_ + read_so_far - eol_len), eol_len) == eol) {
      // EOL found
      lines.push_back(
          string(reinterpret_cast<const char *>(buffer_ + start_of_line),
                 read_so_far - start_of_line));
      start_of_line = read_so_far;
    }
    if (read_so_far == size) {
      if (start_of_line != read_so_far) {
        lines.push_back(
            string(reinterpret_cast<const char *>(buffer_ + start_of_line),
                   read_so_far - start_of_line));
      }
      break;  // Reached the maximum read length
    }
  }
  return lines;
}

size_t
SerialX::write(const string &data) {
  ScopedWriteLock lock(this->pimpl_);
  return this->write_(reinterpret_cast<const uint8_t *>(data.c_str()),
                      data.length());
}

size_t
SerialX::write(const std::vector<uint8_t> &data) {
  ScopedWriteLock lock(this->pimpl_);
  return this->write_(&data[0], data.size());
}

size_t
SerialX::write(const uint8_t *data, size_t size) {
  ScopedWriteLock lock(this->pimpl_);
  return this->write_(data, size);
}

size_t
SerialX::write_(const uint8_t *data, size_t length) {
  return pimpl_->write(data, length);
}

void SerialX::setPort(const string &port) {
  ScopedReadLock rlock(this->pimpl_);
  ScopedWriteLock wlock(this->pimpl_);
  bool was_open = pimpl_->isOpen();
  if (was_open) close();
  pimpl_->setPort(port);
  if (was_open) open();
}

string
SerialX::getPort() const {
  return pimpl_->getPort();
}

void SerialX::setTimeout(serialx::Timeout &timeout) {
  pimpl_->setTimeout(timeout);
}

serialx::Timeout
SerialX::getTimeout() const {
  return pimpl_->getTimeout();
}

void SerialX::setBaudrate(uint32_t baudrate) {
  pimpl_->setBaudrate(baudrate);
}

uint32_t
SerialX::getBaudrate() const {
  return uint32_t(pimpl_->getBaudrate());
}

void SerialX::setBytesize(bytesize_t bytesize) {
  pimpl_->setBytesize(bytesize);
}

bytesize_t
SerialX::getBytesize() const {
  return pimpl_->getBytesize();
}

void SerialX::setParity(parity_t parity) {
  pimpl_->setParity(parity);
}

parity_t
SerialX::getParity() const {
  return pimpl_->getParity();
}

void SerialX::setStopbits(stopbits_t stopbits) {
  pimpl_->setStopbits(stopbits);
}

stopbits_t
SerialX::getStopbits() const {
  return pimpl_->getStopbits();
}

void SerialX::setFlowcontrol(flowcontrol_t flowcontrol) {
  pimpl_->setFlowcontrol(flowcontrol);
}

flowcontrol_t
SerialX::getFlowcontrol() const {
  return pimpl_->getFlowcontrol();
}

void SerialX::flush() {
  ScopedReadLock rlock(this->pimpl_);
  ScopedWriteLock wlock(this->pimpl_);
  pimpl_->flush();
}

void SerialX::flushInput() {
  ScopedReadLock lock(this->pimpl_);
  pimpl_->flushInput();
}

void SerialX::flushOutput() {
  ScopedWriteLock lock(this->pimpl_);
  pimpl_->flushOutput();
}

void SerialX::sendBreak(int duration) {
  pimpl_->sendBreak(duration);
}

void SerialX::setBreak(bool level) {
  pimpl_->setBreak(level);
}

void SerialX::setRTS(bool level) {
  pimpl_->setRTS(level);
}

void SerialX::setDTR(bool level) {
  pimpl_->setDTR(level);
}

bool SerialX::waitForChange() {
  return pimpl_->waitForChange();
}

bool SerialX::getCTS() {
  return pimpl_->getCTS();
}

bool SerialX::getDSR() {
  return pimpl_->getDSR();
}

bool SerialX::getRI() {
  return pimpl_->getRI();
}

bool SerialX::getCD() {
  return pimpl_->getCD();
}
