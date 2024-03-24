# malloc.
Nos apoyamos en la estructura de region para la implementacion de alocacion de memoria en el heap. Las estructuras usadas para almacenar los bloques (conjunto de regiones reservadas con el mismo mmap) son simplemente regiones doblemente enlazadas, y que cuentan con un identificador.

Nuestra estructura de region, al tenerla enlazada con regiones anteriores y siguientes que pueden o no pertenecer al mismo bloque, debio de contar con un identificador. Las regiones ademas cuentan con una referencia a las regiones adyencentes a ella, y en conjunto con el identificador de bloque, permiten su coleasing y splitting. Dos regiones deben unirse si estan libres, son adyacentes y pertenecen al mismo bloque.

Al tener distintos tipos de tamanios de bloque, y al no saber dentro de que lista se hallan, contamos por cada region con un enum que sirve de identificador: nos dice si el bloque es de tipo Small, Medium, o Large. Elegimos un enum del tipo y no que guarde el tamanio entero del bloque al que pertenece para reducir el espacio que ocupa este tipo de dato. Al tener este enum y el tamanio de la region en particular, es facil saber cuando hay que liberar al bloque, realizando un munmap. Esto sucede cuando el tamanio de la region + el tamanio del header son iguales al tamanio del bloque establecido. Este enum tambien nos sirve para actualizar las estadisticas de cada una de las listas por separado.

Enlazamos las regiones en relacion a su tipo de bloque, guardando una referencia al primero de cada tipo, para facilitar la busqueda de una region libre que cuente con el tamanio requerido por el usuario. Sabemos que si se pide, por ejemplo, mas memoria que lo que tiene un bloque pequenio entero (restando un header, que seria el tamanio total mas grande real), entonces no vale la pena buscar en las regiones pequenias, buscaremos solo en la lista de medianas y grandes. Para ahorranos algunos recorridos de la regiones innecesariamente, almacenamos en una variable adicional la cantidad de memoria disponible en cada lista de regiones. El hecho de que esta variable diga que en toda la lista hay mas memoria de la que necesitamos no nos garantiza que encontraremos un unico espacio que la contenga, es decir, que nos sirva, pero si ahorra algunos cuantos recorridos.

Dados los tipos de dato almacenados en un struct region, podemos estimar que el tamanio de uno de estos es de aproximadamente 32 bytes. Es por esto que elegimos como tamanio minimo de region 128 bytes, ya que representa 4 veces el tamanio del header, por lo que ademas es tambien multiplo de 4, lo que lo hace requerible a mmap.


# calloc
Esta funcion realiza un malloc, realiza un memset en 0 de la memoria pedida por el usuario. Por mas que la memoria aportada por mmap viene inicializada en 0, no vale la pena chequear si este espacio habia sido ya ocupado para ver si debiamos limpiar la memoria o no, tomamos la decision de siempre llenarlo de 0's.

# realloc
Esta funcion recibe un puntero a una region anterior, y pide que se la mueva a otra con un nuevo tamanio. En vez de agarrar todos los datos actuales y moverlos a una region del nuevo tamanio pedido disponible, primero nos fijamos si podemos mantener la informacion donde ya esta, realizando un coleasce o un split. Si el nuevo tamanio pedido es menor al que tenia antes, entonces simplemente recorta la region en la que ya esta. Si el nuevo tamanio pedido es mayor que el que ya tenia, y si la region continua hacia la derecha (es decir, la region siguiente), alcanza para llegar al tamanio buscado, entonces se devuelve esta. Si no es suficiente, no debo mirar en la siguiente nuevamente, ya que si estuviese libre se hubiese hecho previamente un coleasce, por lo que se pasa al ultimo caso. En este ultimo lo que se hace es un nuevo alloc, haciendo uso de nuestro malloc, se copia el contenido de la region anterior del usuario hasta el minimo tamanio entre la region anterior y la nueva y se libera la region vieja.

# first fit
El criterio de seleccion del first fit es bastante autoexplicativo. Este va buscando en todas las listas, empezando por la de menor tamanio en el que podria entrar, y se queda con la primer region que cumple con el tamanio pedido.

# best fit
Tomamos como criterio que el best fit implicase unicamente recorrer en el mismo bloque, es decir, una vez que encuentra una region disponible, sigue recorriendo hasta terminar de recorrer el bloque, no la lista entera, para ver si el tamanio de alguna de las otras regiones disponibles de este tienen un tamanio mas cercano al pedido. No optamos por recorrer toda la lista ya que implicaba aumentar demasiado la complejidad computacional de la busqueda, y consideramos que no valia la pena.

# nombres de las funciones
Debido a que teslib.c, entre otros archivos, hacen uso de la biblioteca stdlib, debimos cambiar los nombres de nuestras funciones ya que a veces usaba el malloc externo, y a veces usaba el nuestro propio. Esto impedia que pudiesemos correr correctamente las pruebas.

# pruebas
Consideramos que podriamos haber probado algunas cosas mas, pero esto implicaba hacer publicas ciertas funciones y estructuras que no queriamos; terminaban siendo muy de caja blanca. Las pruebas se pueden correr ejecutando el comando "make valgrind".