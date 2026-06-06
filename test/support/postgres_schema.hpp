#ifndef TEST_SUPPORT_POSTGRES_SCHEMA_HPP
#define TEST_SUPPORT_POSTGRES_SCHEMA_HPP

namespace odb {
class database;
}

namespace im::test {

void EnsureCoreSchema(odb::database& db);

} // namespace im::test

#endif // TEST_SUPPORT_POSTGRES_SCHEMA_HPP
