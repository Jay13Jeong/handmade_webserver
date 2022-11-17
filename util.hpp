#include <iostream>
#include <string>
#include <vector>
#include <cstring>
// #include <algorithm>

namespace util
{
	bool remove_last_semicolon(std::string &line)
	{
		if (line.back() == ';')
		{
			line.pop_back();
			return (true);
		}
		return (false);
	}

	std::vector<std::string> ft_split(std::string s, std::string divid) {
		std::vector<std::string> v;
		if (s.length() == 0)
			return v;
		char* c = strtok((char*)s.c_str(), divid.c_str());
		while (c) {
			v.push_back(c);
			c = strtok(NULL, divid.c_str());
		}
		return v;
	}

	std::vector<std::string> ft_split_s(std::string s, std::string divid)
	{
		std::vector<std::string> v;
		size_t pos;
		if (s.length() == 0)
			return v;
		pos = s.find(divid);
		while (pos != std::string::npos)
		{
			v.push_back(s.substr(0, pos));
			s = s.substr(pos + divid.length(), s.length() - v.back().length() + divid.length());
			pos = s.find(divid);
		}
		if (s.length() != 0)
			v.push_back(s);
		return v;
	}

	bool is_numeric(std::string str)
	{
		char* p;
		strtol(str.c_str(), &p, 10);
		return *p == 0;
	}

	int count_semicolon(std::string str)
	{
		int ret = 0;
		for (int i = 0; i < str.length(); i++)
		{
			if (str[i] == ';')
				ret++;
		}
		return (ret);
	}
}