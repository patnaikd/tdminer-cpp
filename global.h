#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define __STR2__(x) #x
#define __STR1__(x) __STR2__(x)
#define __LOC__ __FILE__"("__STR1__(__LINE__)") : Warning Msg: "

#endif /* _GLOBAL_H_ */
