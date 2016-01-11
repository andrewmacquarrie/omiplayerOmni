#pragma once

#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

class TrackCamera
{
public:
	TrackCamera();
	~TrackCamera();

	std::string const& operator[](std::size_t index) const;

	std::size_t size() const;

	void readNextRow(std::istream& str);

private:
	std::vector<std::string>    m_data;
};
