#include "stdafx.h"
#include "trackCamera.h"

TrackCamera::TrackCamera(){

}
TrackCamera::~TrackCamera(){

}

std::string const& TrackCamera::operator[](std::size_t index) const
{
	return m_data[index];
}
std::size_t TrackCamera::size() const
{
	return m_data.size();
}
void TrackCamera::readNextRow(std::istream& str)
{
	std::string         line;
	std::getline(str, line);

	std::stringstream   lineStream(line);
	std::string         cell;

	m_data.clear();
	while (std::getline(lineStream, cell, ','))
	{
		m_data.push_back(cell);
	}
}


