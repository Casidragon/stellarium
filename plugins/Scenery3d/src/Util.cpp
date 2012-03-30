#include "Util.hpp"
#include <fstream>
#include <sstream>

using namespace std;

void trim_right(string& source, const string& t)
{
    size_t found;
    found = source.find_last_not_of(t);
    if (found != string::npos)
    {
        source.erase(found + 1);
    }
    else
    {
        source.clear();
    }
}

void trim_right(string& source)
{
    string trim_chars(" \t\f\v\n\r");
    trim_right(source, trim_chars);
}

vector<string> splitStr(const string& line, char delim)
{
	 size_t cutAt;
    string str(line);
    vector<string> results;
    while ((cutAt = str.find_first_of(delim)) != str.npos)
    {
        if (cutAt > 0)
        {
            results.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt + 1);
    }
    if (str.length() > 0)
    {
        results.push_back(str);
    }
    return results;
}

float parseFloat(const string& str)
{
    stringstream ss(str);
    float f = 0;
    ss >> f;
    if (ss.fail()) {
        throw (str + " is not a number");
    }
    return f;
}

unsigned int parseInt(const string& str)
{
    stringstream ss(str);
    unsigned int i = 0;
    ss >> i;
    if (ss.fail()) {
        throw (str + " is not a number");
    }
    return i;
}

void makeNormalMatrix(std::vector<float> &mat)
{

}

const char* getTime()
{
    QByteArray byteArray = QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8();
    const char* cString = byteArray.constData();
    return cString;
}

void parseTextureString(std::string in, std::string &out)
{
    //Copy in into out
    out = in;
    //Remove leading space
    if(out.at(0) == ' ')
        out = out.substr(1, out.length()-1);

    //Remove trailing space
    trim_right(out);

    //Replace black slashes with slashes
    size_t position = out.find("\\");
    while (position != std::string::npos)
    {
        out.replace(position, 1, "/");
        position = out.find("\\", position + 1);
    }
}

Vec3f vecdToFloat(Vec3d v)
{
    Vec3f out;
    out.v[0] = static_cast<float>(v.v[0]);
    out.v[1] = static_cast<float>(v.v[1]);
    out.v[2] = static_cast<float>(v.v[2]);

    return out;
}

Vec3d vecfToDouble(Vec3f v)
{
    Vec3d out;
    out.v[0] = static_cast<double>(v.v[0]);
    out.v[1] = static_cast<double>(v.v[1]);
    out.v[2] = static_cast<double>(v.v[2]);

    return out;
}

