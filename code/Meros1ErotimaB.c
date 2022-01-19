#include <unistd.h>
#include "string.h"
#include <sys/wait.h>
#include <stdio.h>     /* printf()                 */
#include <stdlib.h>    /* exit(), malloc(), free() */
#include <sys/types.h> /* key_t, sem_t, pid_t      */
#include <sys/shm.h>   /* shmat(), IPC_RMID        */
#include <errno.h>     /* errno, ECHILD            */
#include <semaphore.h> /* sem_open(), sem_destroy(), sem_wait().. */
#include <fcntl.h>     /* O_CREAT, O_EXEC          */
#include <stdbool.h>

/*--------------------------------------------------
           ΥΛΟΠΟΙΗΣΗ BAKERY ALGORITHM
--------------------------------------------------*/

/*συνάρτηση που επιστρέφει το μέγιστο στοιχείο ενός πίνακα*/
int largest(int arr[], int n)
{
    int i;
    int max = arr[0];
    for (i = 1; i < n; i++)
        if (arr[i] > max)
            max = arr[i];

    return max;
}

int main()
{
    int n = 1; /*αρχικ΄ή τιμή του n σε περίπτωση που γίνει λάθος είσοδος*/
    /*απλό user input για τον αριθμό n*/
    printf("Enter a number of processes: ");
    scanf("%d", &n);

    key_t shmkey; //κλειδί κοινής μνήμης
    int shmid;    //identifier κοινής μνήμης
    pid_t pid[n]; //πίνακας με όλα τα process id
    int i;        //loop index
    int child_status;

    struct shared /*όλα τα shared variables ενωμένα σε ένα struct 
                    (για ευκολότερη δημιουργία του shared memory space)*/
    {
        bool choosing[n]; /*δείχνει αν μια διεργασία επιλέγει αυτή τη στιγμή νούμερο true ή false*/
        int number[n];    /*τρέχον νούμερο μιας διεργασίας*/
        int SA[n];        /*ο κοινός πόρος, ένα απλό array*/
    };

    shmkey = ftok("/dev/null", 6);                                   /*δημιουργία ενός κλειδιού κοινής μνήμης*/
    shmid = shmget(shmkey, sizeof(struct shared), 0644 | IPC_CREAT); /*ανάθεση κοινής μνήμης ίσης με το μέγεθος ένος "struct shared"*/

    /*attach της κοινής μνήμης σε ένα struct shared και αρχικοποίηση των πεδίων του σε 0*/
    struct shared *shr = (struct shared *)shmat(shmid, (void *)0, 0);
    memset((void *)shr->choosing, false, sizeof(shr->choosing));
    memset((void *)shr->number, 0, sizeof(shr->number));
    memset((void *)shr->SA, 0, sizeof(shr->SA));

    /*εκτελείται όσες φορές πει ο χρήστης*/
    for (i = 0; i < n; i++)
    {
        pid[i] = fork(); //δημιουργία διεργασίας

        if (pid[i] == 0) //τρέχει μόνο στο child process
        {
            /*σε αυτό το σημείο είναι η ουσιαστική υλοποίηση του bakery algorithm*/
            int j = 0;                                    //loop index
            shr->choosing[i] = true;                      //αλλαγή του chosing σε true
            shr->number[i] = largest(shr->number, n) + 1; /*επιλογή αριθμο΄ύ προτεραιότητας ίσου με
                                                             το μέγιστο των αριθμών των άλλων διεργασιών*/
            shr->choosing[i] = false;                     //αλλαγή του chosing σε false
            for (j = 0; j < n; j++)                       //loop αναμονής προτεραιότητας, διατρέχει κάθε διεργασία
            {
                //όσο η διεργασία j επιλέγει
                while (shr->choosing[j] == true)
                    ; //no-op

                do
                    ; //no-op
                /*όσο η διεργασία j δεν έχει νούμερο μηδέν και (number[i], i)>(number[j], j)
                 δηλαδή όσο η j έχει προτεραιότητα*/
                while ((shr->number[j] != 0) && (shr->number[i] > shr->number[j] || (shr->number[i] == shr->number[j] && i > j)));
            }
            //κρίσιμο τμήμα
            for (j = 0; j < n; j++)
            {
                shr->SA[j] = shr->SA[j] + i; //απλώς αύξηση κάθε θέσης του πίνακα κατά i
            }
            shr->number[i] = 0; //θέτουμε το νούμερο σε 0, μή κρίσιμο τμήμα
            shmdt(shr);         //detach κοινής μνήμης
            exit(0);            //exit της διεργασίας-παιδί
        }
        else if (pid[i] < 0) //σε περίπτωση fork error
        {
            printf("egine lathos fork\n");
        }
    }

    /*αυτό το loop τρέχει μόνο στο parent process αφού όλα τα child έχουν κάνει exit*/
    for (i = 0; i < n; i++) //για κάθε παιδί
    {
        pid_t wpid = waitpid(pid[i], &child_status, 0); //αναμονή μέχρι να τελειώσει και αποθήκευση του status
        if (WIFEXITED(child_status))                    //αν έκανε exit χωρίς error
        {
            //exit message με status code
            printf("Child %d terminated with exit status %d\n", wpid,
                   WEXITSTATUS(child_status));
        }
        else //αν έκανε exit με error
        {
            //exit message με status code
            printf("Child %d terminated abnormally\n", wpid);
        }
    }
    //αφού τελείωσαν τα process, κοιτάμε τι έχει ο πίνακας
    for (i = 0; i < n; i++) //για κάθε θέση του πίνακα
    {
        //εκτύπωση του περιεχομένου
        printf("Array index %d has value %d\n", i, shr->SA[i]);
    }
    shmdt(shr);                 //detach κοινής μνήμης
    shmctl(shmid, IPC_RMID, 0); //mark shared memory segment to be destroyed

    return 0;
}
