#include <iostream>
#include <string>
#include <queue>
#include <array>
#include <vector>
#include <utility>

#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cctype>
#include <cstdint>

#include <chrono>
#include <deque>
#include <iomanip>
#include <algorithm>


constexpr int MAX_CONCURRENT_DOWNLOADS = 300; 
constexpr std::uint64_t MAX_PAGES_TO_CRAWL = 20'000;

unsigned int crawl_count {0};

struct Page 
{
    std::string url;
    std::string content;
};

struct DownloadTask 
{
    CURL* easy_handle = nullptr;
    Page page;
    bool active = false;
};

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata); 

void parsePage(Page&& page, std::queue<std::string>& urlQueue);

int main()  
{
    std::queue<std::string> urlQueue;
	
        urlQueue.push("https://www.imdb.com/list/ls006131498");
    urlQueue.push("https://en.wikipedia.org/wiki/Cartoon_Network");
    urlQueue.push("https://en.wikipedia.org/wiki/List_of_animated_feature_films");
    urlQueue.push("https://www.discovermagazine.com/planet-earth/20-of-the-most-adorable-animals-to-help-make-anyones-day");
    urlQueue.push("https://www.nick.com/shows/dora-the-explorer");
    urlQueue.push("https://dora.fandom.com/wiki/Dora_the_Explorer");
    urlQueue.push("https://www.commonsensemedia.org/tv-reviews/dora-the-explorer");
    urlQueue.push("https://www.imdb.com/title/tt7547410/");
    urlQueue.push("https://www.rottentomatoes.com/browse/movies_at_home/genres:kids_and_family");
    urlQueue.push("https://www.netflix.com/browse/genre/783");
    urlQueue.push("https://www.directv.com/insider/top-cartoons-for-kids/");
    urlQueue.push("https://www.disneyplus.com/browse/kids");
    urlQueue.push("https://www.vudu.com/content/movies/kids-family");
    urlQueue.push("https://www.youtube.com/@NickJr");
    urlQueue.push("https://en.wikipedia.org/wiki/Dora");
    urlQueue.push("https://github.com/LoayAhmed304/search-engine");
    urlQueue.push("https://github.com");
    urlQueue.push("https://medium.com");
    urlQueue.push("https://www.geeksforgeeks.org");
    urlQueue.push("https://www.bbc.com/news");
    urlQueue.push("https://www.theverge.com");
    urlQueue.push("https://en.wikipedia.org/wiki/Cartoon_Network");
    urlQueue.push("https://www.discovermagazine.com/planet-earth/20-of-the-most-adorable-animals-to-help-make-anyones-day");
    urlQueue.push("https://leetcode.com/problemset");
    urlQueue.push("https://www.reddit.com/r/learnprogramming");


    // M - Curl initialization
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    {
        std::cerr << "curl_global_init failed\n";
        return 1;
    }

    // M - Multi handle initialization
    CURLM* multi_handle = curl_multi_init();
    if(!multi_handle) 
    {
        std::cerr << "curl_multi_init failed\n";
        curl_global_cleanup();
        return -1;
    }
    
    // M - Setting the maximum number of concurrent connections for the multi handle, casted to long to match the expected type for the option.
    curl_multi_setopt(multi_handle, CURLMOPT_MAX_TOTAL_CONNECTIONS, static_cast<long>(MAX_CONCURRENT_DOWNLOADS));

    // M - Creating an array of DownloadTask structures of size maximum concurrent downloads, each containing an easy handle and a Page object.
    DownloadTask tasks[MAX_CONCURRENT_DOWNLOADS];

    // M - Looping through each task to initialize the easy handle and set various options for the download.
    for(int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++) 
    {
        tasks[i].easy_handle = curl_easy_init();
        if (!tasks[i].easy_handle) 
        {
            std::cerr << "curl_easy_init failed for task " << i << "\n";
            return -1;
        }
        // M - Setting the write callback function and the data pointer for the easy handle to store the downloaded content in the Page object.
        curl_easy_setopt(tasks[i].easy_handle, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(tasks[i].easy_handle, CURLOPT_WRITEDATA, &tasks[i].page);

        // M - Setting additional options for the easy handle, such as following redirects, maximum number of redirects, user agent string, and accepting all encodings.
        curl_easy_setopt(tasks[i].easy_handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(tasks[i].easy_handle, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(tasks[i].easy_handle, CURLOPT_USERAGENT, "MyCrawler/1.0");
        curl_easy_setopt(tasks[i].easy_handle, CURLOPT_ACCEPT_ENCODING, "");
    }

    // M - Variables to track the number of running handles, pages started, and successful pages.
    // AI 
        int running = 0;
        std::uint64_t pagesStarted = 0;
        std::uint64_t successfulPages = 0;
        using Clock = std::chrono::steady_clock;
        auto startTime = Clock::now();
        auto lastReportTime = startTime;

        // Stores the timestamps of successful downloads
        // from the last 60 seconds.
        std::deque<Clock::time_point> successfulFetchTimes;
    // AI

	auto start = std::chrono::steady_clock::now();
	size_t next_print {1000};
    while(true)
    {
        // M - Step 1: Looping on each task to check if it is active or not, if it's not acitve we pop an URL from the queue and set it for the task, then add the easy handle to the multi handle.
        for (int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++)
        {
            if (urlQueue.empty() || pagesStarted >= MAX_PAGES_TO_CRAWL)
                break;

            if (tasks[i].active)
                continue;

            tasks[i].page.url = urlQueue.front();
            urlQueue.pop();

            tasks[i].page.content.clear(); // Clear previous content

            // M - Setting the URL.
            CURLcode rc = curl_easy_setopt(tasks[i].easy_handle, CURLOPT_URL, tasks[i].page.url.c_str());
            if (rc != CURLE_OK)
            {
                std::cerr << "curl_easy_setopt failed for task " << i << "\n";
                continue;
            }
            // M - Adding the easy handle to the multi handle.
            CURLMcode MC = curl_multi_add_handle(multi_handle, tasks[i].easy_handle);
            if (MC != CURLM_OK) 
            {
                std::cerr << "curl_multi_add_handle failed for task " << i << "\n";
                continue;
            }
            tasks[i].active = true;
            ++pagesStarted;
        }

        // M - Step 2: Start donwloading downloads and collect completed ones.
        curl_multi_perform(multi_handle, &running);

        int messagesLeft = 0;
        CURLMsg* message = nullptr;

        // M - curl_multi_info_read(), it basically asks the multi handle if there is any messages from the individual transfers. 
        // Each call returns a new struct until a special "end of msgs" struct is returned as a signal that there is no more to get at this point.
        // That 'message' contains the easy handle itself and basic information like the result of the transfer.
        // Returns: A pointer to a filled-in struct, or NULL if it failed or ran out of structs. It also writes the number of messages left in thequeue (after this read) in the integer the second argument points to.

        while ((message = curl_multi_info_read(multi_handle, &messagesLeft)))
        {
            if (message->msg != CURLMSG_DONE)
                continue;

            CURL* finished_handle = message->easy_handle;
            CURLcode result = message->data.result;

            // M - Due to the struct returning only contains the easy handle, we need to find the corresponding DownloadTask in our array of tasks.
            DownloadTask* finished_task = nullptr;
            for(int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++) 
            {
                if(tasks[i].easy_handle == finished_handle) 
                {
                    finished_task = &tasks[i];
                    break;
                }
            }

            if(!finished_task)
                continue;
            
            
            long httpStatus = 0;
            char* effectiveUrl = nullptr;
	    //std::cout<<" "<<pagesStarted<<std::endl;

            // Get the HTTP response status.
            curl_easy_getinfo(finished_handle, CURLINFO_RESPONSE_CODE, &httpStatus);
            // M - Could be used later in the database

                // Get the final URL after redirects.
                curl_easy_getinfo(finished_handle, CURLINFO_EFFECTIVE_URL, &effectiveUrl);

                // Copy the URL into a C++ string while the libcurl pointer is valid.
                std::string finalUrl;

                if (effectiveUrl != nullptr)
                {
                    finalUrl = effectiveUrl;
                }

            // Remove the completed easy handle from the multi handle.
            curl_multi_remove_handle(multi_handle, finished_handle);
            finished_task->active = false;
            
            // AI
                if (result == CURLE_OK && httpStatus >= 200 && httpStatus < 300)
                {
                    // Increment total successful downloads.
                    ++successfulPages;
                    // Record when this page was successfully downloaded.
                    successfulFetchTimes.push_back(Clock::now());
                    parsePage(std::move(finished_task->page), urlQueue);
                    crawl_count++;
                }
                else
                  //  std::cerr << "HTTP/Download error: " << finished_task->page.url << " | " << curl_easy_strerror(result) << " | HTTP " << httpStatus << '\n';
            // AI
            
            // Clear the page content.
            finished_task->page = Page{};
        }
        
        // AI 
            auto now = Clock::now();
            // Remove successful downloads older than 60 seconds.
            while (!successfulFetchTimes.empty() &&
                now - successfulFetchTimes.front() >= std::chrono::seconds(60))
            {
                successfulFetchTimes.pop_front();
            }

            // Update the counter approximately every second.
            if (now - lastReportTime >= std::chrono::seconds(1))
            {
                double elapsed = std::chrono::duration<double>(
                    now - startTime
                ).count();

                // During startup, calculate the rate using the actual elapsed time.
                // After 60 seconds, use the rolling 60-second window.
                double windowSeconds = std::min(60.0, elapsed);
                double pagesPerMinute = 0.0;

                if (windowSeconds > 0.0)
                {
                    pagesPerMinute =
                        successfulFetchTimes.size() * 60.0 / windowSeconds;
                }
               // std::cout << "\rSuccessful pages: " << successfulPages << " | Started: " << pagesStarted << " | Current rate: " << std::fixed << std::setprecision(1) << pagesPerMinute << " pages/min" <<std::endl<< std::flush;
                lastReportTime = now;
            }
            if (crawl_count >= next_print)
        {
        double mins = std::chrono::duration<double>(
                          std::chrono::steady_clock::now() - start).count() / 60.0;
        std::cout << crawl_count / mins << " pages/min ("
                  << crawl_count << " pages "<<"\n";
        next_print += 1000;
        }

        // AI


        if (running == 0 && (urlQueue.empty() || pagesStarted >= MAX_PAGES_TO_CRAWL))
            break;

        if(running > 0)
            curl_multi_poll(multi_handle, nullptr, 0, 1000, nullptr); // Wait for activity or timeout, recommened by documentation to avoid busy waiting if curl_multi_perform() was used.
    }

   // std::cout << '\n';

    // M - Cleanup is done be removing each easy handle at first, clean it up and then clean the multi handle and the global curl.
    for (int i = 0; i < MAX_CONCURRENT_DOWNLOADS; i++)
    {
        if (tasks[i].active)
            curl_multi_remove_handle(multi_handle, tasks[i].easy_handle);

        curl_easy_cleanup(tasks[i].easy_handle);
    }

    curl_multi_cleanup(multi_handle);
    curl_global_cleanup();
    return 0;
}

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    Page* page = static_cast<Page*>(userdata);
    page->content.append(ptr, size * nmemb);
    return size * nmemb;
}

// AI
    std::string trim(const std::string& str)
{
    size_t start = 0;
    size_t end = str.size();

    while (start < end &&
           std::isspace(static_cast<unsigned char>(str[start])))
    {
        ++start;
    }

    while (end > start &&
           std::isspace(static_cast<unsigned char>(str[end - 1])))
    {
        --end;
    }

    return str.substr(start, end - start);
}
    bool resolveHttpUrl(CURLU* urlHandle, const std::string& baseUrl, const std::string& reference, std::string& result)
    {
    // Start from the original base URL every time.
    CURLUcode rc = curl_url_set(
        urlHandle,
        CURLUPART_URL,
        baseUrl.c_str(),
        0
    );

    if (rc != CURLUE_OK)
        return false;

    // Resolve the reference against that base.
    rc = curl_url_set(
        urlHandle,
        CURLUPART_URL,
        reference.c_str(),
        0
    );

    if (rc != CURLUE_OK)
        return false;

    // Only allow HTTP and HTTPS URLs.
    char* scheme = nullptr;

    rc = curl_url_get(
        urlHandle,
        CURLUPART_SCHEME,
        &scheme,
        0
    );

    if (rc != CURLUE_OK)
        return false;

    std::string schemeString(scheme);
    curl_free(scheme);

    std::transform(
        schemeString.begin(),
        schemeString.end(),
        schemeString.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );

    if (schemeString != "http" && schemeString != "https")
        return false;

    // Fragments are not sent in HTTP requests.
    // Remove them so different #sections don't become separate URLs.
    curl_url_set(
        urlHandle,
        CURLUPART_FRAGMENT,
        nullptr,
        0
    );

    // Extract the resolved URL.
    char* absoluteUrl = nullptr;

    rc = curl_url_get(
        urlHandle,
        CURLUPART_URL,
        &absoluteUrl,
        0
    );

    if (rc != CURLUE_OK)
        return false;

    result.assign(absoluteUrl);

    curl_free(absoluteUrl);

    return true;
}

    // Returns the first direct child element of "parent" with the given tag name.
    static xmlNodePtr findChildElement(xmlNodePtr parent, const char* name)
{
    if (parent == nullptr)
        return nullptr;

    for (xmlNodePtr child = parent->children; child != nullptr; child = child->next)
    {
        if (child->type == XML_ELEMENT_NODE &&
            xmlStrcasecmp(child->name, reinterpret_cast<const xmlChar*>(name)) == 0)
        {
            return child;
        }
    }

    return nullptr;
}

    // Collects every descendant element of "root" with the given tag name,
    // in document order. Iterative so deeply nested pages cannot overflow the stack.
    static void collectElementsByTagName(xmlNodePtr root, const char* name, std::vector<xmlNodePtr>& out)
{
    if (root == nullptr)
        return;

    xmlNodePtr node = root->children;

    while (node != nullptr)
    {
        if (node->type == XML_ELEMENT_NODE &&
            xmlStrcasecmp(node->name, reinterpret_cast<const xmlChar*>(name)) == 0)
        {
            out.push_back(node);
        }

        // Depth-first traversal.
        if (node->children != nullptr)
        {
            node = node->children;
        }
        else
        {
            while (node != nullptr && node != root && node->next == nullptr)
                node = node->parent;

            if (node == nullptr || node == root)
                break;

            node = node->next;
        }
    }
}

    // Reads an attribute into a std::string. Returns false if it does not exist.
    static bool getAttribute(xmlNodePtr element, const char* name, std::string& value)
{
    xmlChar* attr = xmlGetProp(element, reinterpret_cast<const xmlChar*>(name));

    if (attr == nullptr)
        return false;

    value.assign(reinterpret_cast<const char*>(attr));
    xmlFree(attr);

    return true;
}

    void parsePage(Page&& page, std::queue<std::string>& urlQueue)
{
    if (page.content.empty())
        return;

    // STEP 1 + 2: Parse the downloaded HTML into a libxml2 document.
    // RECOVER keeps going on malformed HTML, NOERROR/NOWARNING silence libxml2's
    // stderr output, NONET forbids network access while parsing.

    htmlDocPtr document = htmlReadMemory(
        page.content.data(),
        static_cast<int>(page.content.size()),
        page.url.c_str(),
        nullptr,
        HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING | HTML_PARSE_NONET
    );

    if (document == nullptr)
    {
        std::cerr << "libxml2 failed to parse: " << page.url << '\n';
        return;
    }

    xmlNodePtr root = xmlDocGetRootElement(document);

    if (root == nullptr)
    {
        xmlFreeDoc(document);
        return;
    }

    xmlNodePtr head = findChildElement(root, "head");
    xmlNodePtr body = findChildElement(root, "body");

    // STEP 3: Determine the base URL.

    // Normally this is the URL of the downloaded page.
    std::string baseUrl = page.url;

    CURLU* urlHandle = curl_url();

    if (urlHandle == nullptr)
    {
        xmlFreeDoc(document);
        return;
    }

    // HTML can contain <base href="...">.
    // If present, relative links should use that URL instead.
    if (head != nullptr)
    {
        std::vector<xmlNodePtr> baseElements;
        collectElementsByTagName(head, "base", baseElements);

        for (xmlNodePtr element : baseElements)
        {
            std::string baseReference;

            if (!getAttribute(element, "href", baseReference))
                continue;

            baseReference = trim(baseReference);

            std::string resolvedBase;

            if (resolveHttpUrl(
                    urlHandle,
                    page.url,
                    baseReference,
                    resolvedBase))
            {
                baseUrl = std::move(resolvedBase);
            }

            // Only the first <base href> is relevant.
            break;
        }
    }

    // STEP 4: Find all <a> elements.

    std::vector<xmlNodePtr> anchors;

    if (body == nullptr)
    {
        curl_url_cleanup(urlHandle);
        xmlFreeDoc(document);
        return;
    }

    collectElementsByTagName(body, "a", anchors);

    // STEP 5: Extract href attributes and resolve them.

    size_t totalLinks = anchors.size();
    size_t addedLinks = 0;

    for (size_t i = 0; i < totalLinks; ++i)
    {
        xmlNodePtr anchor = anchors[i];

        // Copy the attribute into a C++ string.
        std::string link;

        // No href attribute.
        if (!getAttribute(anchor, "href", link))
            continue;

        link = trim(link);

        // Ignore empty links and fragment-only links.
        if (link.empty() || link.front() == '#')
            continue;

        // Resolve into an absolute HTTP/HTTPS URL.
        std::string absoluteUrl;

        if (!resolveHttpUrl(
                urlHandle,
                baseUrl,
                link,
                absoluteUrl))
        {
            continue;
        }

        // Add the URL to the crawler's frontier.
        urlQueue.push(std::move(absoluteUrl));

        ++addedLinks;
    }

    // STEP 6: Cleanup.
    /*
    std::cout
        << "Parsed: " << page.url
        << " | Anchors found: " << totalLinks
        << " | URLs added: " << addedLinks
        << '\n';
    */
    curl_url_cleanup(urlHandle);

    xmlFreeDoc(document);
}
// AI





// void easyApproach() {

//     // Initialize libcurl globally
//     CURLcode rc = curl_global_init(CURL_GLOBAL_ALL);
//     if (rc != CURLE_OK) {
//         std::cerr << "curl init failed\n";
//         return;
//     }
//     else {
//         std::cout << "curl init succeeded\n";
//     }

//     CURL* handle = curl_easy_init();
//     if(handle == NULL) {
//         std::cerr << "curl_easy_init failed\n";
//         curl_global_cleanup();
//         return;
//     }
//     else {
//         std::cout << "curl_easy_init succeeded\n";
//     }

//     curl_easy_setopt(handle, CURLOPT_URL, "https://www.google.com");
//     CURLcode success = curl_easy_perform(handle);

//     if(success != CURLE_OK) {
//         std::cerr << "Download failed: " << curl_easy_strerror(success) << std::endl;
//     }

//     curl_easy_cleanup(handle);
//     curl_global_cleanup();
// }
