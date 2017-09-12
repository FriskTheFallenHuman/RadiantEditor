#pragma once

/**
 * greebo: SortedFilenames is based on a std::set container 
 * with special sorting, since the idTech4 virtual filesystem
 * sorts PK4 archives in reverse alphabetical order, i.e. a PK4 file
 * named zyx.pk4 has higher precedence than abc.pk4
 */

// Arnout: note - sort pakfiles in reverse order. This ensures that
// later pakfiles override earlier ones. This because the vfs module
// returns a filehandle to the first file it can find (while it should
// return the filehandle to the file in the most overriding pakfile, the
// last one in the list that is).
class PakLess
{
public:
	/*!
		This behaves identically to stricmp(a,b), except that ASCII chars
		[\]^`_ come AFTER alphabet chars instead of before. This is because
		it converts all alphabet chars to uppercase before comparison,
		while stricmp converts them to lowercase.
	*/
	bool operator()(const std::string& self, const std::string& other) const 
	{
		const char* a = self.c_str();
		const char* b = other.c_str();

		while (true)
		{
			char c1 = toUpper(*a++);
			char c2 = toUpper(*b++);

			if (c1 < c2)
			{
				return false; // a < b
			}

			if (c1 > c2)
			{
				return true; // a > b
			}

			if (c1 == '\0')
			{
				// greebo: End of first string reached, strings are equal
				return false; // a == b && a == 0
			}
		}
	}

private:
	inline char toUpper(char c) const
	{
		if (c >= 'a' && c <= 'z')
		{
			return c - ('a' - 'A');
		}

		return c;
	}
};

/**
 * Filenames are sorted in reverse alphabetical order
 * using the PakLess comparator.
 */
typedef std::set<std::string, PakLess> SortedFilenames;
