
#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>

// https://stackoverflow.com/questions/1120140/how-can-i-read-and-parse-csv-files-in-c
class CSVRow
{
    public:
        std::string_view operator[](std::size_t index) const
        {
            return std::string_view(&m_line[m_data[index] + 1], m_data[index + 1] -  (m_data[index] + 1));
        }
        std::size_t size() const
        {
            return m_data.size() - 1;
        }
        void readNextRow(std::istream& str)
        {
            std::getline(str, m_line);

            m_data.clear();
            m_data.emplace_back(-1);
            std::string::size_type pos = 0;
            while((pos = m_line.find(',', pos)) != std::string::npos)
            {
                m_data.emplace_back(pos);
                ++pos;
            }
            // This checks for a trailing comma with no data after it.
            pos   = m_line.size();
            m_data.emplace_back(pos);
        }
    private:
        std::string         m_line;
        std::vector<int>    m_data;
};

std::istream& operator>>(std::istream& str, CSVRow& data)
{
    data.readNextRow(str);
    return str;
}   

class CSVIterator
{   
    public:
        typedef std::input_iterator_tag     iterator_category;
        typedef CSVRow                      value_type;
        typedef std::size_t                 difference_type;
        typedef CSVRow*                     pointer;
        typedef CSVRow&                     reference;

        CSVIterator(std::istream& str)  :m_str(str.good()?&str:NULL) { ++(*this); }
        CSVIterator()                   :m_str(NULL) {}

        // Pre Increment
        CSVIterator& operator++()               {if (m_str) { if (!((*m_str) >> m_row)){m_str = NULL;}}return *this;}
        // Post increment
        CSVIterator operator++(int)             {CSVIterator    tmp(*this);++(*this);return tmp;}
        CSVRow const& operator*()   const       {return m_row;}
        CSVRow const* operator->()  const       {return &m_row;}

        bool operator==(CSVIterator const& rhs) {return ((this == &rhs) || ((this->m_str == NULL) && (rhs.m_str == NULL)));}
        bool operator!=(CSVIterator const& rhs) {return !((*this) == rhs);}
    private:
        std::istream*       m_str;
        CSVRow              m_row;
};

class CSVRange
{
    std::istream&   stream;
    public:
        CSVRange(std::istream& str)
            : stream(str)
        {}
        CSVIterator begin() const {return CSVIterator{stream};}
        CSVIterator end()   const {return CSVIterator{};}
};


// own code by Erik Hofman
class JSBSim
{
    public:
        std::string name;
        std::vector<int> date;
        std::vector<int> time;
        std::vector<double> alpha;
        std::vector<double> beta;
        std::vector<double> CDi, CDo, CDtot, CDtrefftz;
        std::vector<double> CFx, CFy, CFz, CL;
        std::vector<double> CMx, CMy, CMz;

        JSBSim(std::istream& file)
        {
            for(auto& row: CSVRange(file))
            {
                char *end;
                const char *start = row[1].data();
                double d = std::strtod(start, &end);
                if (row[0] == "Results_Name") {
                        name = row[1];
                } else if (row[0] == "Results_Date") {
                    push_vector<int>(date, row);
                } else if (row[0] == "Results_Time") {
                    push_vector<int>(time, row);
                } else if (row[0] == "Alpha") {
                    alpha.push_back(d);
                } else if (row[0] == "Beta") {
                    beta.push_back(d);
                } if (row[0] == "CDi") {
                    CDi.push_back(d);
                } if (row[0] == "CDo") {
                    CDo.push_back(d);
                } if (row[0] == "CDtot") {
                    CDtot.push_back(d); 
                } if (row[0] == "CDtrefftz") {
                    CDtrefftz.push_back(d);
                } if (row[0] == "CFx") {
                    CFx.push_back(d);
                } if (row[0] == "CFy") {
                    CFy.push_back(d);
                } if (row[0] == "CFz") {
                    CFz.push_back(d);
                } if (row[0] == "CL") {
                    CL.push_back(d);
                } if (row[0] == "CMx") {
                    CMx.push_back(d);
                } if (row[0] == "CMy") {
                    CMy.push_back(d);
                } if (row[0] == "CMz") {
                    CMz.push_back(d);
                }
            }
        }
        ~JSBSim() = default;

    private:
    template <typename T>
    void push_vector(std::vector<T>& dst, const CSVRow& row) {
        for (int i=1; i<row.size(); ++i)
        {
            char *end; 
            const char *start = row[i].data();
            double d = std::strtod(start, &end);
            dst.push_back(d);
        }
    }
};

int main(int argc, char *argv[])
{
    std::ifstream file(argv[1]);
    JSBSim jsb(file);

    std::cout << "Name: " << jsb.name << std::endl;

    std::cout << std::setprecision(3);

    std::cout << "CL:" << std::endl;
    for (int a=0; a<jsb.alpha.size(); ++a) {
        std::cout << std::setw(6) << jsb.alpha[a] << "  ";
        std::cout << std::setw(6) << jsb.CL[a] << std::endl;
    }
}
