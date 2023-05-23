Quentin                                             
ROD


# Système de fichiers

L'objectif de ce laboratoire est de réaliser un programme qui attend sur des évênements matériels et les traite. 

Il consiste à moduler la fréquence d'une LED par le biais de boutons poussoirs. La fréquence de clignotement par défaut est à 2Hz. Un bouton permet de l'augmenter, un de le diminuer et un permettant de le remettre à la fréquence initiale.

L'attente se fait passivement grâce au multiplexage des entrées. Contrairement au polling,le processus attend qu'une donnée soit disponible.

Les boutons et les leds du NanoHat sont connectés aux GPIO du Nano-Pi. Ces derniers sont mis à disposition par le kernel sous la forme de fichier dans "/sys/class/gpio".

## Ouverture des fichiers des périphériques

### Boutons et leds
Afin d'avoir accès aux fichiers de configuration des GPIO, il est nécessaire d'écrire dans "/sys/class/gpio/export" le numéro de la pin à exporter.

Un dossier est crée pour chaque pin. Il contient des fichiers pour pouvoir le contrôler.

La configuration est la suivante:
- Led : "direction" : output

- Boutons : "direction" : input
            "edge" : rising

L'écriture de "rising" dans le fichier "edge" permet d'indiquer qu'une interuption doit être générée lors d'un flanc montant lors de l'appui sur le bouton.


Les descripteurs utilisés par epoll sont ceux du fichier "value" des boutons.

### Timer
La gestion de la fréquence de clignotement s'effectue avec un timer. La fonction timerfd_create(CLOCK_MONOTONIC, 0) crée un timer en utilisant une horloge précise et retourne un descripteur de fichier. 
La fonction timerfd_settime configure le timer avec une structure "itimerspec" qui spécifie la durée entre deux interruptions.


IMAGE OBJECTIF 

## Mise en place de l'attente passive

Un contexte est premièrement créé avec epoll_create1(0). Il est ensuite possible d'ajouter les différents évênements avec la fonction epoll_ctl(). Il est spécicifé le contexte, l'opération d'ajout, le descripteur de fichier du périphérique, une structure epoll_event.

Cette fonction est appelée pour les trois boutons ainsi que le timer.

La structure epoll_event a plusieurs utilités. Premièrement, elle indique l'évênement que l'on attend sur le descripteur de fichier. Elle contient également un membre permettant de mettre une donnée choisie.

Lorsqu'une donnée est disponible, la fonction epoll_wait, fournit la structure epoll_event du descripteur de fichier correspondant. Le membre contenant une donnée personnelle doit permettre d'identifier le descripteur de fichier.

Dans le code proposé, la donnée personnelle utilisée est le descripteur de fichier.

Les évênements spécifiés pour chacun sont :
- Boutons : EPOLLIN | EPOLLET : Attente d'une donnée disponible en lecture ainsi qu'un flanc.
- Timer : EPOLLIN : Attente d'une donnée disponible en lecture.


## Attente passive et action correspondante

La fonction epoll_wait() attend passivement qu'une donnée soit disponible parmis les descripteurs de fichier du contexte. Elle est appelée dans une boucle infinie afin de revenir en état d'attente dès qu'un évênement est traité.

Lorsque le processus sort de la fonction, la structure epoll_event est remplie. Le membre contenant la donnée personnelle permet d'identifier le périphérique et l'action correspondant est effectuée.


## Affichage dans les logs

La nouvelle fréquence est affichée dans les logs avec la fonction syslog. Le niveau choisi est LOG_INFO. Les logs sont situés dans "/var/log/messages". 
La fonction openlog est appelée en spécifiant l'identifiant qui apparait au début de chaque message, soit "csel_syslog".

Je ne savais pas qu'un tel mécanisme était disponible sur Linux. Dans le cadre d'un projet personnel sur Linux, je souhaitais utiliser un encodeur rotatif. L'utilisation du polling provoquait un résultat catastrophique. Le multiplexage d'entrée aurait sûrement permis de diminuer la latence. J'ai apprécié que le mécanisme de multiplexage soit appliqué sur des périphériques embarqués. J'ai eu des difficultés à utiliser "struct itimerspec" pour la configuration d'un timer. Je ne comprennais pas la différence entre "it_interval" et "it_value".



# Multiprocessing et Ordonnanceur : Processus, signaux et communication
L'objectif de ce travail pratique est de communiquer entre un processus parent et enfant grâce à un mécanisme de Linux. Le mécanisme pour cet exercice est socketpair.

L'enfant communique au parent des chaines de caractères qui sont affichées par ce dernier. Pour les deux, les signaux sont interceptés. Egalement, chaque processus doit fonctionner sur un coeur différent.

La première étape consiste à intercepter les signaux autre que SIGKILL et SIGSTOP. La fonction utilisée est signal et prend en paramètre le numéro du signal ainsi que la callback. L'interception des signaux est héritée par le processus enfant suite à un "fork". En revanche, cela n'est plus le cas si ce dernier effectue un "exec".

Ensuite un socket bidirectionnel est créé avec la fonction socketpair() et comme paramètre SOCK_STREAM. Elle fournit deux descripteurs de fichiers.

Le processus enfant est créé au travers de l'appel à fork(). Il s'agit d'une copie du processus parent. La valeur retournée par fork() permet de faire la distinction entre les deux processus. Elle est 0 dans le cas de l'enfant.

Lors d'un fork, les descripteurs de fichiers sont également dupliqués. Le processus enfant faisant uniquement de l'écriture et le parent de la lecture, chacun ferme le descripteur non nécessaire.

Le processus enfant écrit dans le socket avec la fonction write() en donnant le descripteur de fichier d'écriture du socket. Le processus parent lit avec la fonction read() en donnant le descripteur de fichier de lecture du socket.

Afin que le processus soit associée à un coeur précis, la fonction sched_setaffinity() est utilisée. Elle prend en paramètre un "cpu_set_t" qui contient l'indice du coeur. Il est configuré avec CPU_SET().

Lors de l'exécution du programme, le signal 17 est capturé. Il correspond à SIGCHLD et est émis lorsque le processus enfant meurt.

Je connaissais déjà les sockets ainsi que la création de processus.Je n'ai pas rencontré de difficultés particulières.

# Multiprocessing et Ordonnanceur : CGroups 1
L'objectif de ce travail pratique est d'expérimenter les cgroups en limitant la mémoire d'un programme.

La première étape consiste à monter les cgroups en spécifiant les sous-systèmes à exporter. Dans le cadre de l'exercice, uniquement la ressource "memory" est à restreindre. Les commandes sont les suivantes:

- mount -t tmpfs none /sys/fs/cgroup
- mkdir /sys/fs/cgroup/memory
- mount -t cgroup -o memory memory /sys/fs/cgroup/memory

Un groupe pour la mémoire est créé : 
- mkdir /sys/fs/cgroup/memory/mem

Le processus courant est ajouté dans le groupe :
- echo $$ > /sys/fs/cgroup/memory/mem/tasks

Le groupe est limité à 20MB : 
- echo 20M > /sys/fs/cgroup/memory/mem/memory.limit_in_bytes

Le programme réalisé alloue un espace mémoire de 1MB dans le tas jusqu'à qu'une erreur soit retournée. Il affiche le nombre d'allocations ainsi qu'un message avant sa terminaison.

1. La commande "echo $$" affiche le PID du processus courant. Le fait de l'écrire dans le fichier "task" permet de l'ajouter au groupe.
L'ensemble des processus enfant est automatiquement intégré au groupe.

2. Le programme se termine lors de la 20 ème allocation. Le message de fin n'étant pas affiché, cela signifie qu'il a été tué par un autre processus. La limite étant de 20MB, cela montre que l'allocation mémoire alloue plus de mémoire que celle demandée.

Il est possible de modifier le comportement lorsque le quota est atteint au travers du fichier " /sys/fs/cgroup/memory/mem/oom_control". Si la valeur est à 0, SIGKILL est envoyé, sinon si il est à 1, le programme est mis en pause tant qu'il n'y a pas de ressources libérées.(https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/6/html/resource_management_guide/sec-memory)

3. Il est possible de surveiller la quantité de RAM consommées par les tâches d'un groupe. Elle est affichée dans le fichier "/sys/fs/cgroup/memory/mem/memory.usage_in_bytes".

Grâce à ce laboratoire j'ai appris à répartir la mémoire RAM d'une machine en sous-groupes. Cela peut-être très utile lors de la réalisation d'une infrastructure système. Je n'ai pas rencontré de difficulté particulière.

# Multiprocessing et Ordonnanceur : CGroups 2
L'objectif de ce travail pratique est de restreindre l'utilisation des coeurs du processeur.

Le programme réalisé crée une tâche enfant avec l'appel à fork(). Le processus parent et enfant effectuent une boucle infinie afin d'occuper toutes les ressources du coeur. L'appui de CTRL+C termine les deux processus.

La première étape consiste à monter les cgroups en spécifiant le sous-système "cpuset". Il permet l'assignation de coeurs ainsi que de mémoire:
mount -t tmpfs none /sys/fs/cgroup
mkdir /sys/fs/cgroup/cpuset
mount -t cgroup -o cpu,cpuset cpuset /sys/fs/cgroup/cpuset

Création de deux groupes :
mkdir /sys/fs/cgroup/cpuset/high
mkdir /sys/fs/cgroup/cpuset/low

Assignation d'un coeur en spécifiant son indice :
echo 3 > /sys/fs/cgroup/cpuset/high/cpuset.cpus
echo 2 > /sys/fs/cgroup/cpuset/low/cpuset.cpus

Assignation d'une mémoire en spécifiant son indice :
echo 0 > /sys/fs/cgroup/cpuset/high/cpuset.mems
echo 0 > /sys/fs/cgroup/cpuset/low/cpuset.mems

1. Si aucun coeur n'est assigné, le programme ne peut pas être exécuté. De même si aucune mémoire RAM n'est présente.

2. Deux shells distincts sont connectés par SSH. L'attribution des processus aux groupes s'effectue respectivement avec echo $$ > /sys/fs/cgroup/cpuset/low/tasks et echo $$ > /sys/fs/cgroup/cpuset/low/tasks. Le programme réalisé est ensuite exécuté sur chacun.
Le comportement esperé est que les coeur 0 et 3 soient occupés à 100%. La vérification est effectuée au travers d'un troisième shell connecté via SSH avec la commande top.

3. Comme il n'est pas possible d'avoir un groupe d'une ressource avec des limitations différentes, deux groupes sont créés.

La première étape consiste à monter les cgroups en spécifiant le sous-système "cpuset". Deux groupes sont créés, "25" et "75" qui occupe respectivement 25% et 75% du coeur: 
mount -t tmpfs none /sys/fs/cgroup
mkdir /sys/fs/cgroup/cpuset
mount -t cgroup -o cpu,cpuset cpuset /sys/fs/cgroup/cpuset
mkdir /sys/fs/cgroup/cpuset/75
mkdir /sys/fs/cgroup/cpuset/25

Assignation du 4ème coeur, soit l'indice 3 :
echo 3 > /sys/fs/cgroup/cpuset/75/cpuset.cpus
echo 3 > /sys/fs/cgroup/cpuset/25/cpuset.cpus

Assignation de la mémoire indice 0 :
echo 0 > /sys/fs/cgroup/cpuset/75/cpuset.mems
echo 0 > /sys/fs/cgroup/cpuset/25/cpuset.mems

L'attribut "cpu.shares" permet d'indiquer la répartition du temps CPU. La valeur ne représente pas une durée mais une proportion:
echo 750 > /sys/fs/cgroup/cpuset/75/cpu.shares
echo 250 > /sys/fs/cgroup/cpuset/25/cpu.shares

Un shell est connecté via SSH et le programme est exécuté. Sur un second shell connecté via SSH, la commande top permet de récupérer le PID du processus parent et enfant. Chaque tâche est assignée à un groupe :
echo  323 > /sys/fs/cgroup/cpuset/75/tasks
echo  324 > /sys/fs/cgroup/cpuset/25/tasks

Grâce à ce laboratoire j'ai appris à restreindre l'utilisation CPU d'une machine en sous-groupes. Cela peut-être très utile lors de la réalisation d'une infrastructure système. Je n'ai pas rencontré de difficulté particulière.