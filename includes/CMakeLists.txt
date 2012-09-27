install(FILES
  File
  Resource
  ResourceManager
  ResourceWatcher
  Service
  Tag
  Variant
  CreateResourceJob
  DataManagement
  DescribeResourcesJob
  SimpleResource
  SimpleResourceGraph
  StoreResourcesJob
DESTINATION ${INCLUDE_INSTALL_DIR}/Nepomuk2 COMPONENT Devel)


install(FILES
  Types/Class
  Types/Literal
  Types/Ontology
  Types/Property
DESTINATION ${INCLUDE_INSTALL_DIR}/Nepomuk2/Types COMPONENT Devel)


install(FILES
  Query/AndTerm
  Query/ComparisonTerm
  Query/FileQuery
  Query/GroupTerm
  Query/LiteralTerm
  Query/NegationTerm
  Query/OptionalTerm
  Query/OrTerm
  Query/Query
  Query/QueryParser
  Query/QueryServiceClient
  Query/ResourceTerm
  Query/ResourceTypeTerm
  Query/Result
  Query/ResultIterator
  Query/SimpleTerm
  Query/StandardQuery
  Query/Term
DESTINATION ${INCLUDE_INSTALL_DIR}/Nepomuk2/Query COMPONENT Devel)


install(FILES
  Vocabulary/NCAL
  Vocabulary/NCO
  Vocabulary/NDO
  Vocabulary/NEXIF
  Vocabulary/NFO
  Vocabulary/NIE
  Vocabulary/NMM
  Vocabulary/NMO
  Vocabulary/NUAO
  Vocabulary/PIMO
  Vocabulary/TMO
DESTINATION ${INCLUDE_INSTALL_DIR}/Nepomuk2/Vocabulary COMPONENT Devel)
