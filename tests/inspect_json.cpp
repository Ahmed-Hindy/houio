#include <houio/HouGeoIO.h>

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "usage: houio_inspect_json <input.geo|bgeo>\n";
        return 1;
    }

    try
    {
        houio::HouGeoIO::makeLog(argv[1], std::cout);
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
