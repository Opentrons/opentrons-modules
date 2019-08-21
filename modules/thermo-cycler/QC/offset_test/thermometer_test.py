import os, sys, time
import serial
import optparse
from datetime import datetime
from datetime import timedelta
#import thermometer library from local directory
sys.path.insert(0, os.path.abspath('../../../../../Mechanical-Test/Equipment'))
#local library
import Eutechnics_4500_driver as EU
import csv

def millis():
   dt = datetime.now() - start_time
   ms = (dt.days * 24 * 60 * 60 + dt.seconds) * 1000 + dt.microseconds / 1000.0
   return ms

if __name__ == '__main__':
    #options to pick from
    parser = optparse.OptionParser(usage='usage: %prog [options] ')
    parser.add_option("--time", dest = "time", type = "int",default = 20, help = "time to run")
    parser.add_option("-t", dest="temp", type = "str", default ="80", help ="target temp")
    parser.add_option("-p", "--port", dest = "port", type = "str", default = 'COM34', help = "thermometer port")
    (options, args) = parser.parse_args(args = None, values = None)

    #Establish  thermometers
    thermometer_H1 = EU.Eutechnics(port = options.port)
    t_end = time.time() + 60 * options.time
    thermometer_H1.setup()
    #thermometer_H1.temp_read()
    #Open CSV file with corresponding headers
    filename = "results/H1_thermometer_data_%s_%sC.csv"%(datetime.now().strftime("%m-%d-%y_%H-%M"), options.temp)
    print(filename)
    with open('{}.csv'.format(filename), 'w', newline='') as data_file:
        test_data={'H1_Temp': None, 'time': None}
        log_file = csv.DictWriter(data_file, test_data)
        log_file.writeheader()
        try:
            start_time = datetime.now()
            print("start_time: ", start_time)
            while time.time() < t_end:
                test_data['H1_Temp'] = thermometer_H1.temp_read()
                test_data['time'] = millis()/1000
                log_file.writerow(test_data)
                print(test_data)
                data_file.flush()
            data_file.close()
            #prompt User
            print("Test done")
            input("Press Enter to close terminal")
        except KeyboardInterrupt:
            print("Test Cancelled")
            test_data['Errors'] = "Test Cancelled"
        except Exception as e:
            print("ERROR OCCURED")
            test_data['Errors'] = e
            raise
