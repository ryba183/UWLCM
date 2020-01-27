from matplotlib import rc
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import AutoMinorLocator, MultipleLocator

import os
import sys
sys.path.append(os.path.dirname(os.path.abspath(__file__)) + "/../../Matplotlib_common/")
from read_UWLCM_arrays import read_my_var

# activate latex text rendering
rc('text', usetex=True)

series_from_it = int(sys.argv[1])
series_to_it = int(sys.argv[2])
profs_from_it = int(sys.argv[3])
profs_to_it = int(sys.argv[4])
qlimit = float(sys.argv[5])

# assumed initial GCCN concentrations
GCCN_conc = [0,0.2817,5*0.2817,10*0.2817]

# init the plot
nplotx = 2 #int(nplots/6 + 0.5)
fig, axarr = plt.subplots(1, nplotx )

#prepare a list of output files, assuming the following order: prsitine, standard, polluted, for each 4 results: no GCCN, GCCN ,GCCNx5, GCCNx10
series_file_names = []
profs_file_names = []
file_no = np.arange(6, len(sys.argv)-1 , 1)
print file_no
for no in file_no:
  series_file_names.append(sys.argv[no] + "series.dat")
  profs_file_names.append(sys.argv[no] + "profiles_"+str(profs_from_it)+"_"+str(profs_to_it)+".dat")

assert(len(series_file_names) == 12)

label_counter=0

for it in np.arange(12):
  if(it % 4 == 0):
    mean_surf_precip = []
    tot_acc_surf_precip = []
    tot_acc_surf_precip_std_dev = []
    prflux = []
    prflux_std_dev = []

  series_infile = open(series_file_names[it], "r")
  profs_infile = open(profs_file_names[it], "r")

# calc mean surf precip
  surf_precip = read_my_var(series_infile, "surf_precip")
  mean_surf_precip.append(np.mean(surf_precip[series_from_it:series_to_it]))
# calc acc surf precip
  acc_surf_precip = read_my_var(series_infile, "acc_precip")
  acc_surf_precip_std_dev = read_my_var(series_infile, "acc_precip_std_dev")
  tot_acc_surf_precip.append(acc_surf_precip[series_to_it] - acc_surf_precip[series_from_it])
  tot_acc_surf_precip_std_dev.append(acc_surf_precip_std_dev[series_to_it] + acc_surf_precip_std_dev[series_from_it])

# find cloud base
  ql = read_my_var(profs_infile, "rliq")
  clbase = np.argmax(ql>qlimit)
#  print ql
  print clbase
# get prflux at cloud base
  prflux.append(read_my_var(profs_infile, "prflux")[clbase])
  prflux_std_dev.append(read_my_var(profs_infile, "prflux_std_dev")[clbase])

  if((it+1) % 4 == 0):
   # tot_acc_surf_precip_std_dev = [3 * x for x in tot_acc_surf_precip_std_dev] # we show errors bars with 3 std dev
    tot_acc_surf_precip = [(24. / 4.) * x for x in tot_acc_surf_precip] # turn into mm / day
    tot_acc_surf_precip_std_dev = [(24. / 4.) * x for x in tot_acc_surf_precip_std_dev] # turn into mm / day
    #axarr[0].plot(GCCN_conc, mean_surf_precip, 'o')
    axarr[0].errorbar(GCCN_conc, tot_acc_surf_precip, yerr = tot_acc_surf_precip_std_dev, marker='o', fmt='.')
    axarr[1].errorbar(GCCN_conc, prflux, yerr = prflux_std_dev, marker='o', fmt='.')

axarr[0].set_xlabel('GCCN concentration [cm$^{-3}$]')
axarr[0].set_ylabel('average surface precipitation rate [mm/day]')

axarr[1].set_xlabel('GCCN concentration [cm$^{-3}$]')
axarr[1].set_ylabel('average precipitation flux at cloud base [W/m$^{2}$]')


# legend font size
#plt.rcParams.update({'font.size': 8})

# hide axes on empty plots
#if nplots % nploty == 0:
#  nemptyplots=0
#else:
#  nemptyplots = nploty - nplots % nploty
#emptyplots = np.arange(nploty - nemptyplots, nploty)
#for empty in emptyplots:
#  axarr[nplotx-1, empty].axis('off')

#axes = plt.gca()
#axes.tick_params(direction='in')
#x_arr = np.arange(nplotx)
#y_arr = np.arange(nploty)
#for x in x_arr:
#  for y in y_arr:
#    #tics inside
#    axarr[x,y].tick_params(direction='in', which='both', top=1, right=1)
#    #minor tics
#    axarr[x,y].xaxis.set_minor_locator(AutoMinorLocator())
#    axarr[x,y].yaxis.set_minor_locator(AutoMinorLocator())
#    #labels and tics font size
#    for item in ([axarr[x,y].xaxis.label, axarr[x,y].yaxis.label] + axarr[x,y].get_xticklabels() + axarr[x,y].get_yticklabels()):
#      item.set_fontsize(8)
#    # subplot numbering
#    if y < nploty - nemptyplots or x < (nplotx - 1):
#      axarr[x,y].text(0.8, 0.9, labeldict[y + x*nploty], fontsize=8, transform=axarr[x,y].transAxes)

## show legends
#for x in np.arange(nplotx):
#  for y in np.arange(nploty):
#    axarr[x,y].legend(loc="upper center")

#single legend for the whole figure
#handles, labels = axarr[0,0].get_legend_handles_labels()
#lgd = fig.legend(handles, labels, handlelength=4, loc='lower center', bbox_to_anchor=(0.45,0))


#figure size
fig.set_size_inches(7.2, 5)# 5.214)#20.75,13.74)
#distances between subplots and from bottom of the plot
#fig.subplots_adjust(bottom=0.14 + (len(labels) - 2) * 0.03, hspace=0.25, wspace=0.4)

#fig.tight_layout(pad=0, w_pad=0, h_pad=0)

#figure size
#fig.set_size_inches(7.874, 6 + (len(labels) - 2) * 0.2)# 5.214)#20.75,13.74)
#distances between subplots and from bottom of the plot
#fig.subplots_adjust(bottom=0.15 + (len(labels) - 2) * 0.02, hspace=0.25)


plt.show()
#fig.savefig(argv[len(sys.argv)-1], bbox_inches='tight', dpi=300)#, bbox_extra_artists=(lgd,))

