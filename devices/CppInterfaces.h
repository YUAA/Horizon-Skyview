//
// CppInterfaces.h
// from http://www.codeproject.com/Articles/10553/Using-Interfaces-in-C#
//

#define Interface class

#define DeclareInterface(name) Interface name { \
          public: \
          virtual ~name() {}

#define DeclareBasedInterface(name, base) class name : public base { \
          public: \
          virtual ~name() {}

#define EndInterface };

#define implements public