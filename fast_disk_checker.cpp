
#define __USE_LARGEFILE64
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <algorithm>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <unistd.h>
#include <vector>


#include <sys/time.h>
double time_diff(struct timeval x, struct timeval y)
{
    double x_ms, y_ms, diff;

    x_ms = (double)x.tv_sec * 1000000 + (double)x.tv_usec;
    y_ms = (double)y.tv_sec * 1000000 + (double)y.tv_usec;

    diff = (double)y_ms - (double)x_ms;

    return diff;
}

//  g++-4.9 -msse -g -o  fast_disk_checker  fast_disk_checker.cpp  -std=c++14 -lpthread ;

using namespace std;
double progress = 0.0;
uint64_t count_errors = 0;
uint64_t count_err_bytes = 0;
int main(int argc, char **args)
{
    static struct timeval before, after;

    vector<uint64_t> eraddr;
    uint64_t fsize = 0;
    uint64_t test_data[8 * 1024];
    uint64_t readed_data[8 * 1024];
    if (argc < 2)
    {
        cout << "using: fast_disk_checker FILEPATH\n";
        return 0;
    }

    fstream fdisk;
    fdisk.open(args[1]);
    if (fdisk.is_open())
    {
        uint64_t pos = fdisk.tellp();
        fdisk.seekg(0, ios_base::end);
        pos = fdisk.tellp();

        //  fsize = 2000000000;
        fsize = pos;

        cout << " ВЫ ДЕЙСТВИТЕЛЬНО ХОТИТЕ ЗАПУСТИТЬ ТЕСТ ДИСКА " << args[1] << " " << (pos / 1024 / 1024 / 1024) << "GB  ?\n"
             << " ВСЕ ДАННЫЕ БУДУТ УДАЛЕНЫ! СТЕРЕТЬ? (y/n) ?";
        char answ = 'n';
        cin >> answ;
        if (answ != 'y')
        {
            cout << " ОТМЕНА ДЕЙСТВИЙ!\n";
            exit(0);
        }
        fdisk.seekg(0, ios_base::beg);
        pos = 0;

        gettimeofday(&before, NULL);

        auto fill_vall = test_data[0];

        std::random_device rd;  //Will be used to obtain a seed for the random number engine
        std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
        std::uniform_int_distribution<> dis(0, 0xFFFFF);
        fill_vall = dis(gen);

        long long iter_cnt = 0;
        cout << " Запись проверочных значений...\n";
        do
        {
            iter_cnt++;
            fill_vall++;
            std::fill(&test_data[0], &test_data[sizeof(test_data) / sizeof(test_data[0])], fill_vall);
            std::fill(&readed_data[0], &readed_data[sizeof(readed_data) / sizeof(readed_data[0])], 0);
            ostream &res = fdisk.write((char *)test_data, sizeof(test_data));
            if (!res.good())
                cout << "error wr in " << pos << endl;

            pos = fdisk.tellp();
            fdisk.flush();

            auto rd_sz = sizeof(test_data);
            if (pos == fsize && fsize % sizeof(test_data))
            {
                rd_sz = fsize % sizeof(test_data);

                cout << " rd sz =" << rd_sz << endl;
                cout << " rd pos =" << fdisk.tellg() << endl;
            }
            //fdisk.seekg(-1 * (int)rd_sz ,ios_base::cur);
            if (pos == fsize - rd_sz)
                break;
            //fdisk.seekp(pos,ios_base::beg);
            progress = (double)pos / (double)fsize;

            if (iter_cnt % 10000 == 0)
                cout << "progress: " << progress << endl;

        } while (pos < fsize - sizeof(readed_data));

        gettimeofday(&after, NULL);

        double tdiff = time_diff(before, after);
        long double sp = ((double)fsize / 1024 / 1024) / (tdiff / 1000000.0);
        cout << " Скорость записи:" << sp << " мегабайт в секунду \n";
    }
    else
        cout << " file not found";
    fdisk.close();

    cout << " Проверка записанных данных ... \n";
    fdisk.open(args[1]);
    if (fdisk.is_open())
    {
        uint64_t count_bl = fsize / sizeof(readed_data);

        cout << " Количество записей = " << count_bl << endl;
        gettimeofday(&before, NULL);
        for (uint64_t i = 0; i < count_bl; i++)
        {
            istream &res_read = fdisk.read((char *)readed_data, sizeof(readed_data));
            if (!res_read.good())
            {
                count_errors++;
                cout << ">>>>>system error rd in " << i << " block " << endl;
            }
            static auto v = readed_data[10];

            int afres = std::accumulate(readed_data, &readed_data[sizeof(readed_data) / sizeof(readed_data[0])], 0);
            if (0 == i)
            {

                //  cout<<" start val = "<<v<<" afres "<<afres<<endl;
            }

            std::fill(&test_data[0], &test_data[sizeof(test_data) / sizeof(test_data[0])], v);
            int test_res = std::accumulate(test_data, &test_data[sizeof(test_data) / sizeof(test_data[0])], 0);
            if (test_res != afres)
            {
                cout << ">>>>> data error rd in " << i << " block " << endl;
                int lecount = 0;
                cout << "\n ************************************* \n";
                for (size_t c = 0; c < sizeof(test_data) / sizeof(test_data[0]); c++)
                {
                    if (readed_data[c] != test_data[c])
                    {
                        // cout<<readed_data[c]<<"!="<<test_data[c]<<"["<<c<<"] ";
                        eraddr.push_back((i * sizeof(test_data)) + c);

                        lecount++;
                        count_err_bytes++;
                    }
                }
                cout << "******************* ercount compare =" << lecount << " ****************** \n";

                count_errors++;
            }
            v++;

            static int prev_progress = 0;
            int pprogress = (int)(1000 * (float)i / (float)count_bl);

            if (pprogress/10 != prev_progress/10)
                cout << "prgs = " << pprogress / 10 << endl;
            prev_progress = pprogress;
        }

        gettimeofday(&after, NULL);

        double tdiff = time_diff(before, after);
        long double sp = ((double)fsize / 1024 / 1024) / (tdiff / 1000000.0);
        cout << " Скорость чтения:" << sp << " мегабайт в секунду \n";
    }
    else
        cout << "open error !\n";

    cout << "\n\n  count_errors " << count_errors << endl;
    cout << " количество ошибочных байтов: " << count_err_bytes << "(" << count_err_bytes / 1024 / 1024 << ")MB" << endl;

    if(eraddr.size()>0)
    { 
      ofstream log;
      log.open("disklog.txt");
      if (log.is_open())
      {
         for (auto it : eraddr)
         {
             log << it << endl;
         }
         log.close();
      }
    }  
}
