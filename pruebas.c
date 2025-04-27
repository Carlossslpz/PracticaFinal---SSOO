

int trans;
int t_espera = 20;

void * buffer()
{
    
    while (1)
    {
        if (trans == 2) break;
        
        //codigo
        //codigo buffer

        while (t_espera != 0)
        {
            if (trans = 1)
            {
                trans = 0;
                break;
            }
            else if (trans == 2) break;
               
            else
            {
                sleep(1);
                t_espera--;
            }
            
        }
        t_espera = 20;
    }

    return NULL;
}


int main()
{

}