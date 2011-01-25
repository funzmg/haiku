/*
 * Copyright 2011, Oliver Tappe <zooey@hirschkaefere.de>
 * Distributed under the terms of the MIT License.
 */


#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <Errors.h>
#include <SupportDefs.h>

#include <package/AddRepositoryRequest.h>
#include <package/Context.h>
#include <package/RefreshRepositoryRequest.h>
#include <package/Roster.h>

#include "MyDecisionProvider.h"
#include "MyJobStateListener.h"
#include "pkgman.h"


using namespace Haiku::Package;


// TODO: internationalization!


static const char* kCommandUsage =
	"Usage: %s add-repo <repo-URL> [<repo-URL> ...]\n"
	"Adds one or more repositories by downloading them from the given URL(s).\n"
	"\n"
;


static void
print_command_usage_and_exit(bool error)
{
    fprintf(error ? stderr : stdout, kCommandUsage, kProgramName);
    exit(error ? 1 : 0);
}


int
command_add_repo(int argc, const char* const* argv)
{
	bool asUserRepository = false;

	while (true) {
		static struct option sLongOptions[] = {
			{ "help", no_argument, 0, 'h' },
			{ "user", no_argument, 0, 'u' },
			{ 0, 0, 0, 0 }
		};

		opterr = 0; // don't print errors
		int c = getopt_long(argc, (char**)argv, "hu", sLongOptions, NULL);
		if (c == -1)
			break;

		switch (c) {
			case 'h':
				print_command_usage_and_exit(false);
				break;

			case 'u':
				asUserRepository = true;
				break;

			default:
				print_command_usage_and_exit(true);
				break;
		}
	}

	// The remaining arguments are repo URLs, i. e. at least one more argument.
	if (argc < optind + 1)
		print_command_usage_and_exit(true);

	const char* const* repoURLs = argv + optind;
	int urlCount = argc - optind;

	MyDecisionProvider decisionProvider;
	Context context(decisionProvider);
	MyJobStateListener listener;
	context.SetJobStateListener(&listener);

	status_t result;
	for (int i = 0; i < urlCount; ++i) {
		AddRepositoryRequest addRequest(context, repoURLs[i], asUserRepository);
		result = addRequest.CreateInitialJobs();
		if (result != B_OK)
			DIE(result, "unable to create necessary jobs");

		while (Job* job = addRequest.PopRunnableJob()) {
			result = job->Run();
			delete job;
			if (result == B_CANCELED)
				return 1;
		}

		BString repoName = addRequest.RepositoryName();
		Roster roster;
		RepositoryConfig repoConfig;
		roster.GetRepositoryConfig(repoName, &repoConfig);
		RefreshRepositoryRequest refreshRequest(context, repoConfig);
		result = refreshRequest.CreateInitialJobs();
		if (result != B_OK)
			DIE(result, "unable to create necessary jobs");

		while (Job* job = refreshRequest.PopRunnableJob()) {
			result = job->Run();
			delete job;
			if (result == B_CANCELED)
				return 1;
		}
	}

	return 0;
}