#pragma once

#include "ilogwriter.h"
#include <streambuf>

namespace applog 
{

/**
 * greebo: The LogStreamBuf adapts the std::streambuf to use to the
 *         LogWriter class for the actual logging.
 */
class LogStreamBuf :
	public std::streambuf
{
	// Internal character buffer
	char* _reserve;

	// The associated level, is passed to the LogWriter
	LogLevel _level;

public:
	/**
	 * greebo: Pass the level and the optional buffersize to the constructor.
	 *         Level can be something like SYS_ERROR, SYS_STANDARD, etc.
	 */
	LogStreamBuf(LogLevel level, int bufferSize = 0);

	// Cleans up the buffer
	virtual ~LogStreamBuf();

protected:
	// These two get called by the base class streambuf and are necessary
	// in order to write the buffer to the device
	virtual int_type overflow(int_type c);
	virtual int_type sync();

private:
	// Writes the buffer contents to the log device
	void writeToBuffer();
};

} // namespace applog
