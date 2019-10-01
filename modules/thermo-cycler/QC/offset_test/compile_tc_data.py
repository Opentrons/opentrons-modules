"""
This test is only meant for the thermocycler data extraction
"""
#Import Data handling Libraries
import pandas as pd
import time
import math
import itertools
import csv
#load the example datasheet
df = pd.read_csv('results/A16_F10_thermometer_data_09-27-19_10-48_target_90C.csv.csv') #Change this file name to original
header = df.head(0)
header_list = []
csv_header = {}
for name in header:
    header_list.append(name)
csv_header.update({'index': None})
for name in header_list:
    csv_header.update({name:None})
print(csv_header)
f10_well = df['Offset']
f10_time = df['time']
threshold = -1
new_list = []
time_list = []
tuple_list = []
index_list = []
f10_temp = []
index_num = 0

#reduce amount of sigficant figures without rounding the value
def truncate(n,decimals = 3):
    multiplier = 10**decimals
    return int(n*multiplier)/multiplier
#-------------------------------------------------------------------------------
"""
This eliminates values that are duplicates.
Reduces the amount of granularity
We might need the max value of the cell
for the heatsink
"""
for index, row in df.iterrows():
    if index > 6848:
        break
    print("testing truncate",f10_well[index+1])
    next_cell = truncate(f10_well[index+1],2)
    print("testing Offset",row['Offset'])
    current_cell = truncate(row['Offset'], 2)
    delta = round(next_cell - current_cell, 3)
    if delta == 0:
        pass
    elif current_cell < threshold:
        pass
    else:
        #print('index: ', index+2, 'time: ', row['time'], 'Offset: ', truncate(current_cell, decimals = 2))
        tuple_list.append((index+2, row['time'], truncate(current_cell, decimals = 2)))
        index_list.append(index+2)
        new_list.append(current_cell)
        time_list.append(row['time'])
        #time.sleep(0.1)
#print(time_list)
print(tuple_list)
T_heatsink = df['T.heatsink']
T = df['HS_time']
row = []
for x in range(2, len(T_heatsink)):
    row.append(x)
#--------------------------------------------------------------------------
cell = 0
new_tuple_list = []
try:
    for element, row, t in zip(T_heatsink, itertools.cycle(row), T):
        #print('row', row, 'element', element, 'time', t)
        if t-0.5 <= time_list[cell] and t+0.2 >= time_list[cell]:
            new_tuple_list.append((index_list[cell], new_list[cell], time_list[cell], element, t))
            print('index: ', index_list[cell],'F10:', new_list[cell], 'time: ', time_list[cell], 'T.sink', element , 'T.heatsink time: ', t)
            cell +=1
except Exception as e:
    pass
    #time.sleep(0.1)
#-----------------------------------------------------------------------
file_name = 'Filtered_90C_nofan_A16_file.csv' #Change this to name your new file
with open(file_name, 'w', newline='') as f:
    test_data = csv_header
    log_file = csv.DictWriter(f, test_data)
    log_file.writeheader()
    #write things Here
    for tup in new_tuple_list:
        test_data['index'] = tup[0]
        test_data['Offset'] = tup[1]
        test_data['time'] = tup[2]
        test_data['T.heatsink'] = tup[3]
        test_data['HS_time'] = tup[4]
        log_file.writerow(test_data)
        f.flush()
