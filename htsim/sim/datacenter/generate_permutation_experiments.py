import sys

def generate_experiment(messagesize,linkspeed,paths,mode,oversub,failed):
    ovs = ""
    if (oversub!=1):
        ovs = "_" + str(oversub)+"_to_1"
    
    print ("connection_matrices/perm_8192n_8192c_",messagesize,"MB.cm",sep='')
    print ("!Experiment 8K permutation, 8K leaf-spine, ",linkspeed,"Gbps, ",paths," paths, ",messagesize,"MB messages, ",mode,sep='')
    print ("!Binary ./htsim_uec")
    idealfct = int(messagesize * 8000 / linkspeed + 9) * oversub
    idealfct = int(idealfct * 64 / (64 - failed + failed / 4))
    print ("!Param -end ",max(4*idealfct,1000),sep='')
    print ("!Param -paths ",paths,sep='')
    print ("!Param -linkspeed ",linkspeed,"000",sep='')
    print ("!Param -topo topologies/leaf_spine_8192_",linkspeed,"g",ovs,".topo",sep='')

    queuesize = int(linkspeed / 4)
    ecnmin = int(queuesize / 4)
    ecnmax = int(3* queuesize / 4)
    print ("!Param -q ",queuesize,sep='')
    print ("!Param -ecn ",ecnmin," ",ecnmax,sep='')
    print ("!Param -cwnd ",int(3*queuesize/2),sep='')
    if failure>0:
        print ("!Param -failed ",failure,sep='')
    
    if (mode == "NSCC"):
        print ("!Param -sender_cc_only")
    elif (mode == "BOTH"):
        print ("!Param -sender_cc")
        
    print ("!tailFCT ",int(idealfct*1.2),sep='')


#connection_matrices/perm_8192n_8192c_YYMB.cm
#!Experiment 8K permutation, 8K leaf-spine, 200Gbps, XX paths, YYMB messages, Both.
#!Binary ./htsim_uec
#!Param -end 400
#!Param -paths XX
#!Param -linkspeed 200000
#!Param -topo topologies/leaf_spine_8192_200g.topo
#!Param -q 50
#!Param -ecn 12 37
#!Param -cwnd 75
#!tailFCT 60


n = len(sys.argv)
i = 1;

if (n<3):
    print ("Expected arguments not supplied. Please specify linkspeed [e.g. 200] and algorithm [NSCC, RCCC or BOTH]; optional argument is oversub ratio (4 and 8 supported); another optional argument is link failure count (applied per rack)")
    sys.exit()

linkspeed = int(sys.argv[1])
mode = sys.argv[2]

oversub = 1
if (n>3):
    oversub = int(sys.argv[3])

failure = 0
if (n>4):
    failure = int(sys.argv[4])

if linkspeed not in (200,400,800):
    print ("Supported linkspeeds are 200,400 and 800, you supplied ", linkspeed)
    sys.exit()

if mode not in ("NSCC","RCCC","BOTH"):
    print ("Supported modes are NSCC, RCCC or BOTH, but you supplied ", mode)
    sys.exit()

if oversub not in (1,4,8):
    print ("Oversub ratio can be 1,4 or 8, but you supplied ", oversub)
    sys.exit()

if failure<0 or failure>64:
    print ("Failure can be in interval 0-64, but you supplied ", failure)

for msgsize in (1,2,4,8,16,32,64,100):
    for paths in (32,64,128):
        generate_experiment(msgsize,linkspeed,paths,mode,oversub,failure);
