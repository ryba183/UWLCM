import numpy as np
from sys import argv

from latex_labels import *
from read_UWLCM_arrays import *

def plot_profiles(var_list, plot_iter, nplotx, nploty, axarr, xscaledict, xlimdict, ylimdict, show_bin=False, suffix='', reference=True, ylabel=''):
  #read my results
  profiles_files_names = []
  profiles_labels = []
  file_no = np.arange(1, len(argv)-1 , 2)
  print file_no
  for no in file_no:
    profiles_files_names.append(argv[no]+suffix)
    profiles_labels.append(argv[no+1])
  
  for var in var_list:
    label_counter = 0
    for file_name in profiles_files_names:
    #  try:
      profiles_file = open(file_name, "r")
      my_pos = read_my_var(profiles_file, "position")
      my_res = read_my_var(profiles_file, var)
   
   #     print 'mean nc in cloud cells: ' , np.mean(my_nc[my_nc>20])
    
      profiles_file.close()
    
      linestyles = ['--', '-.', ':']
      dashList = [(3,1),(1,1),(4,1,1,1),(4,2)] 
      if(var == "base_prflux_vs_clhght"):
        plot_my_array(axarr, plot_iter, my_res, my_pos, nploty, xlabel=var_labels[var], ylabel="cloudy column height [m]", varlabel=profiles_labels[label_counter], dashes = dashList[label_counter % len(dashList)], xscale=xscaledict[var], xlim=xlimdict[var], ylim=ylimdict[var])
      else:
        plot_my_array(axarr, plot_iter, my_res, my_pos, nploty, xlabel=var_labels[var], ylabel=ylabel, varlabel=profiles_labels[label_counter], dashes = dashList[label_counter % len(dashList)], xscale=xscaledict[var], xlim=xlimdict[var], ylim=ylimdict[var])
   #   except:
   #     print 'error opening file: ', file_name
   #     my_pos = 0
   #     my_res = 0
      label_counter = label_counter+1
    plot_iter += 1
  return plot_iter

