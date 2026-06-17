# Questions Subsidiaires - IOT

### 1. Expliquer le rôle du scheduler, la différence entre tâche et fonction, l'intérêt de vTaskDelay(), pourquoi l'architecture multitâche est préférable ici ?
  
Le scheduler décide quelle tache occupe le CPU
Une fonction est un bloc de code qu'on appelle et qui rend la main quand il a fini.
Une tâche est un fil d'exécution indépendant, avec sa propre pile, qui tourne en parallèle et ne se termine jamais. 
vTaskDelay() met la tâche en pause sans bloquer le processeur pour les autres taches.

On fait plusieurs choses en même temps. Sans une architecture multitache on bloquerai toutes les opérations dans la l’attente de la fin d’une seule. 

### 2. Que signifie pour une tâche donnée cette instruction : vTaskDelay(pdMS_TO_TICKS(1000)); ? Décrire les différentes étapes de la tâche lorsqu’elle rencontre cette instruction ? Que se passe t’il au niveau des autres tâches en parallèle ?

pdMS_TO_TICKS(1000) convertit 1000 ms en nombre de ticks FreeRTOS. 

##### Etapes :
    1. La tâche n’est plus en cours et passe en bloqué pendant le temps demandé.
    2. La tache la plus prioritaire est lancé ensuite
    3. Au bout des 1000ms la tache est relancé 

Pendant cette seconde, les autres tâches s'exécutent normalement 

### 3. Justifier les priorités FreeRTOS choisies pour chaque tâche dans le rapport technique

La tâche réseau (priorité 3) est la plus prioritaire : elle doit publier les mesures, gérer la reconnexion et recevoir les commandes sans retard, elle maintient le lien avec le serveur.
 
Les tâches acquisition et serveur web (priorité 2) : le DHT22 ne se lit que toutes les 2 secondes. Étant à la même priorité, elles se partagent équitablement le CPU.

La tâche supervision (priorité 1) : elle ne fait que surveiller le système en arrière-plan.

### 4. Pourquoi presque toujours au moins un vTaskDelay(...) dans une tâche

Une tache est une boucle infini, sans le vTaskDelay elle ne rend jamais la main. Les taches qui ont des priorités inférieur ne seront donc jamais lancé.

### 5. 

### 6. Que va-t-il se passer lorsque l’on met dans le code : TaskMQTT priorité 20 TaskSensors priorité 1 Le système sera-t-il forcément meilleur et plus rapide, justifier ?

Non. Si TaskMQTT ne se bloque jamais, elle préempte en permanence TaskSensors, qui ne s'exécute jamais.
Une priorité élevée n'est bénéfique que si la tâche cède régulièrement la main.

### 7. Variable globale partagée

Il y a un probleme d’accès concurrent, TaskSensors écrit pendant que TaskWeb la lit. 
 
### 8. Mutex / Queue / Sémaphore

  - Mutex : un verrou garantissant qu'une seule tâche à la fois accède à une ressource partagée.
  - Sémaphore : c’est un signal de synchronisation, une tâche attend le signal, une autre tâche le donne pour l'autoriser à continuer.
  - Queue :  une file d'attente où une tâche dépose des données et une autre les récupère dans l'ordre.

Mutex plutôt que Queue quand : on veut protéger l'accès à une ressource partagée. La Queue sert à transférer de la donnée d'un producteur vers un consommateur.

### 9. TaskSensors produit temp, TaskMQTT la publie — pourquoi une queue est meilleure ?

Avec une queue, les données restent en attente dans la file tant qu'elles ne sont pas traitées par TaskMQTT. Rien n'est perdu ni écrasé : chaque mesure est conservée jusqu'à ce qu'elle soit publiée, puis retirée de la file

### 10. Que signifie Starvation dans les mécanismes de tâches ? Pouvez-vous donner un exemple ?

C'est la situation où une tâche n'obtient rarement le CPU parce que des tâches plus prioritaires le monopolisent.
  
Exemple : TaskMQTT en priorité 20 tourne sans vTaskDelay. TaskSensors en priorité 1 est prête mais le scheduler choisit toujours la plus prioritaire → TaskSensors ne s'exécute jamais 

### 11. Pourquoi FreeRTOS est-il utilisé dans ce TP alors qu'une simple boucle Arduino pourrait suffire ?

Avec une simple loop(), tout s'exécute l'un après l'autre : si une opération est lente, tout le reste attend.

FreeRTOS permet de faire tourner chaque fonction en parallèle, indépendamment, avec des priorités pour que le plus important passe d'abord.