#pragma once

#include <QString>

#include <git2/errors.h>
#include <git2/global.h>
#include <git2/repository.h>

namespace GitRepo {

void initForCurrentThread();
void shutdownForCurrentThread();
git_repository* openRepository(const QString& path);
void closeRepository(git_repository *repo);

} // namespace GitRepo
