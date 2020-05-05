// Debug function conditionals

#ifdef DEBUG
  #define HTMLDEBUG
  #define D(x)    Serial.print(x)
#else
  #define D(x)
#endif

#ifdef HTMLDEBUG
  const char displayMode[] = "";
#else
  const char displayMode[] = "display: none;";
#endif
