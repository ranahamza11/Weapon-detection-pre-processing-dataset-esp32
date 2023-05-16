#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include<iostream>
#include<fstream>
#include <wchar.h>
#include <time.h>
#include <string>

#define STB_IMAGE_WRITE_IMPLEMENTATION

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 230454
#define DEFAULT_PORT "27015"

//#define ESP_SERVER_IP "192.168.125.29" //old cam
#define ESP_SERVER_IP "192.168.5.29" //new cam

void pixelDifference(const struct Image*, int* [(DEFAULT_BUFLEN - 54) / 3]);
void computeBackground(const struct Image*, int* [(DEFAULT_BUFLEN - 54) / 3], struct Image*);
void foregroundBackgroundDiff(const struct Image*, const struct Image*, struct Image*, struct Image*, struct Image*, struct CropedBox*);
void croppedImage(const struct Image&, struct CropedBox*&, struct Image* out);
bool saveBitmap(char[], struct Image*, int&, int&, int, int);
void writeFile(const std::string&, const struct Image*, int, int, bool);
int readFileCount(const std::string&);
void xAxisIntervals(const struct Image*, struct XAxisIntervalList&, bool);
void findCoordinatesOfBoxes(struct XAxisIntervalList&, const struct Image&, struct CropedBoxList&);
void highlightObjects(const struct CropedBoxList&, struct Image&);
void separateBoundary(const struct Image&, struct XAxisIntervalList&, struct Image*);
bool resize(const struct Image&, struct Image&);

struct Clock {
    clock_t start;
    clock_t end;
    float seconds;
    const float BACKGROUND_EXPIRE_TIME = 600000.0f;
    Clock() {
        start = clock();
        end = clock();
        seconds = 0.0f;
    }

    float timePassedInSecond() const {
        return seconds;
    }

    bool isTimeExpire() {
        if (seconds > BACKGROUND_EXPIRE_TIME) {
            start = clock();
            return true;
        }

        return false;
    }

    void tick() {
        end = clock();
        seconds = (float)(end - start) / CLOCKS_PER_SEC;
    }
};

struct Image {
    unsigned char* frame = nullptr;
    int size = 0;

    int getWidth() const {
        if (frame)
            return *(int*)&frame[18];

        return 0;
    }

    int getHeight() const {
        if (frame)
            return (*(int*)&frame[22] * -1);
        return 0;
    }

    void setWidth(int width) {
        if (frame)
            *(int*)&frame[18] = width;
    }

    void setHeight(int height) {
        if (frame)
            *(int*)&frame[22] = height;
    }
};

struct Pos { //use for crop image
    int x = 0, y = 0;
};

struct XAxisInterval { //use for crop image
    struct Pos start;
    struct Pos end;

    struct XAxisInterval* next = nullptr;
};

struct XAxisIntervalList { //use for crop image
    struct XAxisInterval** list;
    int size = 0;
    int height;
    XAxisIntervalList(int height) {
        this->height = height;
        list = new XAxisInterval * [height];
        for (int i = 0; i < height; i++) {
            list[i] = nullptr;
        }
    }

    bool addInterval(int rowCount, const struct Pos& start, const struct Pos& end) {


        if (list[rowCount] == nullptr) {
            list[rowCount] = new XAxisInterval();
            list[rowCount]->start.x = start.x;
            list[rowCount]->start.y = start.y;

            list[rowCount]->end.x = end.x;
            list[rowCount]->end.y = end.y;

            list[rowCount]->next = nullptr;

            return true;
        }

        struct XAxisInterval* temp = list[rowCount];
        while (temp->next != nullptr) {
            temp = temp->next;

        }

        temp->next = new XAxisInterval();
        temp = temp->next;

        temp->start.x = start.x;
        temp->start.y = start.y;

        temp->end.x = end.x;
        temp->end.y = end.y;

        temp->next = nullptr;
        return true;
    }

    bool popInterval(int rowCount) {

        if (!list[rowCount]) {
            return false;
        }

        list[rowCount] = list[rowCount]->next;

        if (!list[rowCount])
            size--;

        return true;

    }

    void emptyList(int newHeight) {
        struct XAxisInterval* temp;
        for (int i = 0; i < height; i++) {
            temp = list[i];

            while (temp != nullptr) {
                list[i] = list[i]->next;
                delete temp;
                temp = list[i];
            }

        }

        size = 0;
        delete[] list;

        height = newHeight;
        list = new XAxisInterval * [newHeight];
        for (int i = 0; i < newHeight; i++) {
            list[i] = nullptr;
        }
    }

    void print() {
        std::cout << "hehehhehhhhehhe\n";
        struct XAxisInterval* temp = nullptr;
        for (int i = 0; i < height; i++) {
            temp = list[i];
            if (temp) {
                while (temp != nullptr) {
                    std::cout << temp->start.x << ", " << temp->start.y << "\t" << temp->end.x << ", " << temp->end.y << "\t\t";
                    temp = temp->next;
                }
                std::cout << "\n";
            }
        }
    }

};

struct CropedBox {
    int up = 250, down = -1, left = 350, right = -1; //not possible initial values
    struct CropedBox* next = nullptr;

    int getWidth() const {
        if (right - left < 0)
            return 0;

        return right - left + 1;
    }

    int getHeight() const {
        if (down - up < 0)
            return 0;

        return down - up + 1;
    }
};

struct CropedBoxList {
    struct CropedBox* list;
    int size = 0;

    CropedBoxList() {
        list = nullptr;
    }

    bool add(int up, int down, int left, int right) {
        size++;
        if (list == nullptr) {
            list = new CropedBox;
            list->left = left;
            list->up = up;
            list->down = down;
            list->right = right;

            list->next = nullptr;
            return true;
        }

        struct CropedBox* temp = list;

        while (temp->next != nullptr) {
            temp = temp->next;
        }

        temp->next = new CropedBox;
        temp = temp->next;

        temp->left = left;
        temp->up = up;
        temp->down = down;
        temp->right = right;

        temp->next = nullptr;

        return true;
    }

    void pop() {
        if (!list)
            return;

        list = list->next;
        size--;
    }

    void pop_by_index(int index) {
        if (index >= size)
            return;

        if (index == 0) {
            pop();
            return;
        }

        struct CropedBox* temp = list;

        for (int i = 0; i < index - 1; i++) {
            temp = temp->next;
        }

        temp->next = temp->next->next;
        size--;
    }

    void print() {
        struct CropedBox* temp = list;

        while (temp != nullptr) {
            std::cout << temp->up << ", " << temp->down << ", " << temp->left << ", " << temp->right << "\n";
            temp = temp->next;
        }
    }
};

int main()
{
    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = nullptr,
        * ptr = nullptr,
        hints;
    const char* sendbuf = "this is a test";
    char recvbuf[DEFAULT_BUFLEN];
    int iResult;
    int recvbuflen = DEFAULT_BUFLEN;


    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(ESP_SERVER_IP, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Attempt to connect to an address until one succeeds
    for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {

        // Create a SOCKET for connecting to server
        ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
            ptr->ai_protocol);
        if (ConnectSocket == INVALID_SOCKET) {
            printf("socket failed with error: %ld\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        // Connect to server.
        iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    // Send an initial buffer
    iResult = send(ConnectSocket, sendbuf, (int)strlen(sendbuf), 0);
    if (iResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    //printf("Bytes Sent: %ld\n", iResult);

    // shutdown the connection since no more data will be sent
    iResult = shutdown(ConnectSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    struct Image background_imgs[10];
    struct Image background;
    struct Image foreground;
    struct Image diff_Image;
    struct Image image_with_boxes;
    struct Image img_diff_matrix;
    struct Image sub_matrix;
    struct Image croped_img;
    struct Image only_boundary;
    struct Image resizedImage;
    struct XAxisIntervalList intervalList(240);
    struct CropedBoxList cropedBoxes;
    int crp_size = 0;
    for (int i = 0; i < 10; i++) {
        background_imgs[i].frame = new unsigned char[DEFAULT_BUFLEN];
    }

    img_diff_matrix.frame = new unsigned char[320 * 240 + 54]{};
    foreground.frame = new unsigned char[DEFAULT_BUFLEN] {};
    diff_Image.frame = new unsigned char[DEFAULT_BUFLEN];
    image_with_boxes.frame = new unsigned char[DEFAULT_BUFLEN];
    background.frame = new unsigned char[DEFAULT_BUFLEN] {};
    background.size = DEFAULT_BUFLEN;

    int img_count = 0;
    int iter = 0;
    bool isBackFound = false;

    struct Clock clock;

    int testCount = 0;
    int remainIndex = 0;
    std::string path = R"(C:\Users\Abdul Rehman\Documents\FYP-I\CP1Project\CP1Project\)";
    //std::string path = R"(D:\fyp part1\visual studio codes\weaponDetectionLatest\weaponDetectionLatest\)";

    do {

        iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);

        if (!isBackFound) {
            if (iResult > 0) {
                std::cout << "=";

                if (saveBitmap(recvbuf, &background_imgs[img_count], remainIndex, iter, iResult, DEFAULT_BUFLEN)) {

                    std::cout << "\n----------------Time passed in seconds: " << clock.timePassedInSecond() << std::endl;
                    iter = 0;
                    img_count++;
                    std::cout << "Image found for background: " << img_count << std::endl;

                    if (img_count == 10) {
                        if (remainIndex < iResult) {
                            saveBitmap(recvbuf, &foreground, remainIndex, iter, iResult, DEFAULT_BUFLEN);
                        }
                        std::cout << "hii\n";

                        //background is computed here
                        int* pixelDiff[10];
                        for (int i = 0; i < 10; i++) {
                            pixelDiff[i] = new int[(DEFAULT_BUFLEN - 54) / 3]{};
                        }
                        std::cout << "1 Width:  " << background_imgs[0].getWidth() << "   Height: " << background_imgs[0].getHeight() << std::endl;

                        pixelDifference(background_imgs, pixelDiff);

                        computeBackground(background_imgs, pixelDiff, &background);

                        writeFile("background.bmp", &background, 0, DEFAULT_BUFLEN, true);


                        img_count = 0;
                        isBackFound = true;
                        for (int i = 0; i < 10; i++) {
                            delete[] pixelDiff[i];
                        }
                    }
                    else
                        saveBitmap(recvbuf, &background_imgs[img_count], remainIndex, iter, iResult, DEFAULT_BUFLEN);

                    remainIndex = 0;
                }



            }
            else if (iResult == 0)
                printf("Connection closed\n");
            else
                printf("recv failed with error: %d\n", WSAGetLastError());
        }
        else {

            if (iResult > 0) {


                if (saveBitmap(recvbuf, &foreground, remainIndex, iter, iResult, 230454)) {

                    testCount++;
                    if (testCount == 1500)
                        return 0;

                    std::cout << "----------------Time passed in seconds: " << clock.timePassedInSecond() << std::endl;
                    std::cout << "Done\n";


                    for (int j = 0; j < DEFAULT_BUFLEN; j++) {
                        image_with_boxes.frame[j] = foreground.frame[j];
                    }

                    foregroundBackgroundDiff(&background, &foreground, &diff_Image, &img_diff_matrix, nullptr, nullptr);
                    std::cout << "Done1\n";

                    xAxisIntervals(&img_diff_matrix, intervalList, false);
                    intervalList.print();

                    std::cout << "Done2\n";
                    findCoordinatesOfBoxes(intervalList, img_diff_matrix, cropedBoxes);

                    std::cout << "suze " << cropedBoxes.size << std::endl;

                    highlightObjects(cropedBoxes, image_with_boxes);


                    crp_size = cropedBoxes.size;
                    struct CropedBox* box = nullptr;
                    int motionCount = readFileCount("motionCount.txt");
                    int trainingCount = readFileCount("trainingCount.txt");
                    std::cout << motionCount << std::endl;
                    std::string personCount = "";
                    std::string underscore = "";
                    std::string fileName;

                    std::cout << "Crp size" << crp_size << " Image no: " << trainingCount << "\n";
                    for (int j = 0; j < crp_size; j++) {
                        box = cropedBoxes.list;
                        //pixel difference matrix calculation
                        std::cout << "jjj" << "\n";
                        foregroundBackgroundDiff(&background, &foreground, nullptr, &img_diff_matrix, &sub_matrix, box);
                        std::cout << "jjj2" << "\n";

                        separateBoundary(sub_matrix, intervalList, &only_boundary);
                        std::cout << "jjj3" << "\n";


                        croppedImage(foreground, box, &croped_img);
                        std::cout << "jjj4" << "\n";

                        if (resize(only_boundary, resizedImage)) {

                            //resized Image saved here
                            
                            fileName = path + R"(Resize\resizedImage_)" + std::to_string(motionCount) + underscore + personCount + ".bmp";
                            writeFile(fileName, &resizedImage, 0, resizedImage.size, true);
                            std::cout << "Resized image saved\n";

                            fileName = path + R"(training\training_)" + std::to_string(trainingCount) + underscore + personCount + ".bmp";
                            writeFile(fileName, &resizedImage, 0, resizedImage.size, true);
                            std::cout << "Resized image saved\n";
                            writeFile("trainingCount.txt", nullptr, ++trainingCount, 0, false);
                        }

                        //boundary of the objects saved here
                        fileName = path + R"(Boundaries\Boundary_)" + std::to_string(motionCount) + underscore + personCount + ".bmp";
                        writeFile(fileName, &only_boundary, 0, only_boundary.size, true);
                        std::cout << "boudry saved\n";

                        //cropped images saved here
                        fileName = path + R"(trainingHelperObjects\Person_)" + std::to_string(trainingCount) + underscore + personCount + ".bmp";
                        writeFile(fileName, &croped_img, 0, croped_img.size, true);
                        if (j == 0) {
                            underscore = "_";
                            personCount = "1";
                        }
                        else {
                            int temp = std::stoi(personCount);
                            personCount = std::to_string(++temp);
                        }
                        cropedBoxes.pop();

                    }

                    if (crp_size > 0) {

                        std::cout << "jjj5" << "\n";
                        intervalList.emptyList(240);
                        std::cout << "jjj6" << "\n";
                    }
                    //motion detection picture saves here
                    fileName = path + R"(MotionDetection\motion_)" + std::to_string(motionCount) + ".bmp";
                    writeFile(fileName, &diff_Image, 0, DEFAULT_BUFLEN, true);

                    //image with highlighted boxes saves here
                    fileName = path + R"(HighlightedImage\imageBoxes_)" + std::to_string(motionCount) + ".bmp";
                    writeFile(fileName, &image_with_boxes, 0, DEFAULT_BUFLEN, true);

                    //file count is write here
                    writeFile("motionCount.txt", nullptr, ++motionCount, 0, false);
                    


                    iter = 0;
                    if (remainIndex < iResult)
                        saveBitmap(recvbuf, &foreground, remainIndex, iter, iResult, 230454);

                    remainIndex = 0;

                }

            }
        }

        clock.tick();
        if (clock.isTimeExpire()) {
            isBackFound = false;
            img_count = 0;

            for (int i = 0; i < iter; i++) {
                background_imgs[img_count].frame[i] = foreground.frame[i];
            }

        }


    } while (iResult > 0);

    // cleanup
    closesocket(ConnectSocket);
    WSACleanup();

    return 0;
}

void writeFile(const std::string& fileName, const struct Image* imageData, int imageCount, int size, bool isBinary) {
    std::fstream fileHandler;
    if (isBinary) {
        fileHandler.open(fileName, std::ios::out | std::ios::binary);
        for (int i = 0; i < size; i++) {
            fileHandler << imageData->frame[i];
        }
        fileHandler.close();
    }
    else {
        fileHandler.open(fileName, std::ios::out);

        if (fileHandler.is_open()) {
            fileHandler << imageCount;
        }
        fileHandler.close();
    }


}

int readFileCount(const std::string& fileName) {

    std::fstream fileHandler;
    std::cout << fileName << "\n";
    fileHandler.open(fileName, std::ios::in);
    std::string recvData;
    if (fileHandler.is_open()) {
        fileHandler >> recvData;
    }
    else {
        std::cout << "file not open\n";
    }
    fileHandler.close();


    return std::stoi(recvData);
}

bool saveBitmap(char data[], struct Image* recv, int& remainIndex, int& iter, int recvSize, int bitmapSize) {
    for (int i = remainIndex; i < recvSize; i++) {
        recv->frame[iter] = (unsigned char)data[i];
        iter++;

        if (iter == bitmapSize) {
            remainIndex = ++i;
            return true;
        }
    }
    return false;
}

void pixelDifference(const struct Image* bgImages, int* out[(DEFAULT_BUFLEN - 54) / 3]) {

    int rgb_merge_i;
    int rgb_merge_j;
    int offset;

    for (int i = 0; i < 10; i++) {

        for (int j = 0; j < 10; j++) {
            if (i == j)
                break;

            for (int k = 0; k < (DEFAULT_BUFLEN - 54) / 3; k++) {
                offset = 54 + (k * 3);
                rgb_merge_i = ((int)bgImages[i].frame[offset] + (int)bgImages[i].frame[offset + 1] + (int)bgImages[i].frame[offset + 2]) / 3;
                rgb_merge_j = ((int)bgImages[j].frame[offset] + (int)bgImages[j].frame[offset + 1] + (int)bgImages[j].frame[offset + 2]) / 3;
                int diff = rgb_merge_i - rgb_merge_j;
                if (rgb_merge_j > rgb_merge_i)
                    diff = rgb_merge_j - rgb_merge_i;

                if (diff < 29)
                    out[i][k]++;

            }
        }
    }
}

void foregroundBackgroundDiff(const struct Image* background, const struct Image* foreground, struct Image* out, struct Image* diff_matrix, struct Image* out_matrix, struct CropedBox* box) {

    if (box && out_matrix) {
        if (out_matrix->frame)
            delete[] out_matrix->frame;


        out_matrix->frame = new unsigned char[box->getWidth() * box->getHeight() + 54];

        for (int i = 0; i < 54; i++) {
            out_matrix->frame[i] = background->frame[i];
        }
    }
    else {

        for (int i = 0; i < 54; i++) {
            diff_matrix->frame[i] = background->frame[i];
        }
    }



    int backgroundGreyScale;
    int foregroundGreyScale;
    int diff = 0;

    if (box && out_matrix) {

        out_matrix->setWidth(box->getWidth());
        out_matrix->setHeight(box->getHeight() * -1);

        int ind = 54;
        int offset = 0;
        for (int i = box->up; i <= box->down; i++) {

            for (int j = box->left; j <= box->right; j++) {
                offset = (i * 320 * 3 ) + (j * 3) + 54;
                backgroundGreyScale = ((int)background->frame[offset] + (int)background->frame[offset + 1] + (int)background->frame[offset + 2]) / 3;
                foregroundGreyScale = ((int)foreground->frame[offset] + (int)foreground->frame[offset + 1] + (int)foreground->frame[offset + 2]) / 3;

                diff = (int)backgroundGreyScale - (int)foregroundGreyScale;
                if (diff < 0)
                    diff = (int)foregroundGreyScale - (int)backgroundGreyScale;


                if (diff > 26) {
                    out_matrix->frame[ind] = 1;
                }
                else {
                    out_matrix->frame[ind] = 0;
                }

                ind++;
            }
        }

        std::cout << "warning analysis   " << box->getWidth() * box->getHeight() + 54 << "  " << ind << std::endl;
    }
    else if (out) {
        

        for (int i = 0; i < 54; i++) {
            out->frame[i] = background->frame[i];
        }

        for (int i = 54; i < DEFAULT_BUFLEN; i += 3) {

            backgroundGreyScale = ((int)background->frame[i] + (int)background->frame[i + 1] + (int)background->frame[i + 2]) / 3;
            foregroundGreyScale = ((int)foreground->frame[i] + (int)foreground->frame[i + 1] + (int)foreground->frame[i + 2]) / 3;

            diff = (int)backgroundGreyScale - (int)foregroundGreyScale;
            if (diff < 0)
                diff = (int)foregroundGreyScale - (int)backgroundGreyScale;


            if (diff > 24) {
                diff_matrix->frame[(i - 54) / 3 + 54] = 1;
                out->frame[i] = 255;
                out->frame[i + 1] = 255;
                out->frame[i + 2] = 255;
            }
            else {
                diff_matrix->frame[(i - 54) / 3 + 54] = 0;
                out->frame[i] = foreground->frame[i];
                out->frame[i + 1] = foreground->frame[i + 1];
                out->frame[i + 2] = foreground->frame[i + 2];
            }

        }
    }

}

void computeBackground(const struct Image* backImages, int* pixelDiff[(DEFAULT_BUFLEN - 54) / 3], struct Image* out) {
    int iter = 0;
    bool isBackPart;
    int offset;

    //copy header
    for (int i = 0; i < 54; i++) {
        out->frame[i] = backImages[0].frame[i];
    }

    //compute remaining background image
    for (int i = 0; i < (DEFAULT_BUFLEN - 54) / 3; i++) {

        offset = i * 3 + 54;
        isBackPart = false;
        for (int j = 0; j < 10; j++) {
            if (pixelDiff[j][i] > 4) {

                out->frame[offset] = backImages[j].frame[offset];
                out->frame[offset + 1] = backImages[j].frame[offset + 1];
                out->frame[offset + 2] = backImages[j].frame[offset + 2];
                isBackPart = true;
                break;
            }
        }

        if (!isBackPart) {
            out->frame[offset] = 255;
            out->frame[offset + 1] = 255;
            out->frame[offset + 2] = 255;
        }

    }
}

void xAxisIntervals(const struct Image* img_matrix_diff, struct XAxisIntervalList& intervalList, bool isSingleIntervalInARow) {
    int offset;
    int neighbours = 0, notNeighbours = 0;
    bool isStartFound = false, isIntervalFound = false;
    struct Pos start, end;
    int extra;
    bool isIntervalIncreased = false;

    std::cout << "\nfunction -> xAxisIntervals " << img_matrix_diff->getHeight() << "    " << img_matrix_diff->getWidth() << "   " << intervalList.height << "\n\n";
    for (int i = 0; i < img_matrix_diff->getHeight(); i++) { //Intervals found here
        offset = i * img_matrix_diff->getWidth() + 54;
        neighbours = 0;
        notNeighbours = 0;
        for (int j = 0; j < img_matrix_diff->getWidth(); j++) {
            if (img_matrix_diff->frame[offset + j] == 1) {
                neighbours++;
                extra = notNeighbours;
                notNeighbours = 0;

                if (neighbours > 5) {

                    if (!isStartFound) { // starting pixel found here
                        start.x = i;
                        start.y = j - (neighbours + extra - 1);
                        //intervalList.(i, start)
                        isStartFound = true;
                    }

                    end.x = i;
                    end.y = j;
                }
            }
            else {
                notNeighbours++;

                if (notNeighbours > 7) {
                    neighbours = 0;

                    if (isStartFound && !isSingleIntervalInARow) {
                        if (!isIntervalFound) {
                            intervalList.size++;
                            isIntervalIncreased = true;
                        }

                        isStartFound = false;
                        intervalList.addInterval(intervalList.size - 1, start, end);
                        isIntervalFound = true;


                    }
                }
            }
        }

        if (!isSingleIntervalInARow) {
            isIntervalFound = false;
            //std::cout << intervalList.size << std::endl;
            if (isStartFound) {
                if (!isIntervalIncreased) {
                    intervalList.size++;
                    isIntervalIncreased = false;
                }
                intervalList.addInterval(intervalList.size - 1, start, end);

            }
        }
        else if (isStartFound) {
            intervalList.size++;
            intervalList.addInterval(intervalList.size - 1, start, end);
        }

        isStartFound = false;
    }

}

void findCoordinatesOfBoxes(struct XAxisIntervalList& intervalList, const struct Image& img_matrix_diff, struct CropedBoxList& cropedBoxes) {
    struct XAxisInterval* temp = nullptr;
    bool isInitialized = false;
    bool isOverlap = false;
    bool isBoxFound = true;
    struct CropedBox* tempBox = nullptr;
    int up = 250, down = -1, left = 350, right = -1;
    int lists = intervalList.size;

    while (intervalList.size != 0) { //Cropped boxes found here

        isBoxFound = true;
        for (int i = 0; i < lists; ++i) {
            temp = intervalList.list[i];
            isOverlap = false;
            if (!temp) {
                continue;
            }

            if (!isInitialized) {
                up = temp->start.x;
                down = temp->start.x;
                left = temp->start.y;
                right = temp->end.y;
                intervalList.popInterval(i);
                isInitialized = true;
                isBoxFound = false;
                continue;
            }

            if (temp->start.x - down > 33)
                continue;

            if (up > temp->start.x) {
                up = temp->start.x;
            }


            if (left > temp->start.y && left <= temp->end.y) {
                isOverlap = true;
                left = temp->start.y;
            }

            if ((left <= temp->start.y && right >= temp->start.y) && (right >= temp->end.y && left <= temp->end.y)) {
                isOverlap = true;
            }

            if (right < temp->end.y && right >= temp->start.y) {
                isOverlap = true;
                right = temp->end.y;
            }

            if (down < temp->start.x && isOverlap) {
                down = temp->start.x;
            }

            if (isOverlap) {
                intervalList.popInterval(i);
                isBoxFound = false;
            }

        }

        if (intervalList.size == 0)
            isBoxFound = true;

        if (isBoxFound) {
            if (down - up + 1 >= 80 && right - left + 1 >= 25) {
                if (cropedBoxes.list) {
                    tempBox = cropedBoxes.list;
                    while (tempBox->next != nullptr)
                        tempBox = tempBox->next;
                    if (up - 33 <= tempBox->down && down + 33 >= tempBox->up && left <= tempBox->right && right >= tempBox->left) {
                        if (up < tempBox->up) {
                            tempBox->up = up;
                        }

                        if (left < tempBox->left) {
                            tempBox->left = left;
                        }

                        if (right > tempBox->right) {
                            tempBox->right = right;
                        }

                        if (down > tempBox->down) {
                            tempBox->down = down;
                        }

                    }
                    else {
                        cropedBoxes.add(up, down, left, right);

                    }
                }
                else {
                    cropedBoxes.add(up, down, left, right);

                }
            }
            isInitialized = false;
        }

    }

    std::cout << "_____________________________oooooooooooooooooo\n\n";
    cropedBoxes.print();

    int width, height;
    int isCroped = false;
    int whitePixelCount = 0;


    int size = cropedBoxes.size;
    std::cout << "H4 " << size << std::endl;

    //filter croped images in this loop
    tempBox = cropedBoxes.list;
    int index = 0;
    for (int k = 0; k < size; k++) {
        width = tempBox->getWidth();
        height = tempBox->getHeight();

        whitePixelCount = 0;
        //if more than 20% pixels are white then we proceed to crop
        for (int i = tempBox->up; i < tempBox->up + height; i++) {

            for (int j = tempBox->left; j < tempBox->left + width; j++) {
                if (img_matrix_diff.frame[i * 320 + j + 54] == 1)
                    whitePixelCount++;

            }
        }

        //if white pixels are more than 20 percent of the box
        if (((float)whitePixelCount / (width * (float)height)) * 100 > 20)
            isCroped = true;
        else
            isCroped = false;

        if (!isCroped) {
            std::cout << "Invalid cropped box :)\n";
            cropedBoxes.pop_by_index(index);
            index--;
        }

        index++;
        tempBox = tempBox->next;
    }

}

void separateBoundary(const struct Image& img_matrix_diff, struct XAxisIntervalList& intervalList, struct Image* out) {
    intervalList.emptyList(img_matrix_diff.getHeight());
    std::cout << "1 " << "\n";
    std::cout << "1 " << intervalList.size << std::endl;

    xAxisIntervals(&img_matrix_diff, intervalList, false);
    std::cout << "2 " << intervalList.size << std::endl;
    intervalList.print();
    std::cout << "2 " << "\n";
    if (out->frame)
        delete[] out->frame;

    std::cout << "3 " << "\n";
    int widthPadding = (4 - img_matrix_diff.getWidth() % 4) % 4;

    out->size = ((img_matrix_diff.getWidth() + widthPadding) * 3) * img_matrix_diff.getHeight() + 54;
    out->frame = new unsigned char[out->size];

    std::cout << "4 " << "\n";
    for (int i = 0; i < 54; i++) {
        out->frame[i] = img_matrix_diff.frame[i];
    }

    out->setWidth(img_matrix_diff.getWidth() + widthPadding);

    int offset = 0;
    struct XAxisInterval* temp;
    int index = 0;
    bool isBoundaryPart = false;
    std::cout << "separate boundary " << out->getHeight() << "  " << out->getWidth() << "\n";
    std::cout << "-------------------uuuuuuuuuu\n";
    for (int i = 0; i < out->getHeight(); i++) {

        temp = intervalList.list[index];
        isBoundaryPart = false;
        for (int j = 0; j < out->getWidth(); j++) {
            offset = i * (out->getWidth() * 3) + 54 + (j * 3);

            //std::cout << offset + 2 << "\t" << out->size << "\t"  << out->getWidth() << "\t" << out->getHeight() << "\t" << img_matrix_diff.getWidth() << "\t" << img_matrix_diff.getHeight() << "\n";
            if (temp) {
                if (i == temp->start.x && (j == temp->start.y || j == temp->end.y)) {
                    std::cout << temp->start.x << " painted " << temp->start.y << std::endl;
                    if (j == temp->end.y) {
                        temp = temp->next;
                    }

                    isBoundaryPart = true;
                    out->frame[offset + 0] = 255;
                    out->frame[offset + 1] = 0;
                    out->frame[offset + 2] = 0;
                }
                else {
                    out->frame[offset + 0] = 0;
                    out->frame[offset + 1] = 0;
                    out->frame[offset + 2] = 0;
                }
            }
            else {
                out->frame[offset + 0] = 0;
                out->frame[offset + 1] = 0;
                out->frame[offset + 2] = 0;
            }
        }
        //offset += 3;

        if (isBoundaryPart)
            index++;
        else if (index < intervalList.size ) { //missing points are handled here
            temp = intervalList.list[index];
            if (temp) {
                offset = i * (out->getWidth() * 3) + 54 + (temp->start.y * 3);
                out->frame[offset + 0] = 255;
                out->frame[offset + 1] = 0;
                out->frame[offset + 2] = 0;

                if (!temp->next) {
                    offset = i * (out->getWidth() * 3) + 54 + (temp->end.y * 3);
                    out->frame[offset + 0] = 255;
                    out->frame[offset + 1] = 0;
                    out->frame[offset + 2] = 0;
                }
                temp = temp->next;
            }
            while (temp) {

                if (temp->next) {
                    temp = temp->next;
                    continue;
                }

                offset = i * (out->getWidth() * 3) + 54 + (temp->end.y * 3);
                out->frame[offset + 0] = 255;
                out->frame[offset + 1] = 0;
                out->frame[offset + 2] = 0;

                temp = temp->next;
            }
        }
    }

    std::cout << intervalList.size << " -------------------   " << index << "   \n";
}

bool resize(const struct Image& orig, struct Image& out) {
    float scale_x = 52.0f / orig.getWidth(); /*  (200 * 240) / (orig.getWidth() * orig.getHeight());*/
    float scale_y = 108.0f / orig.getHeight();
    //if (scale_x < 1.0f || scale_y < 1.0f)
    //    return false;

    if (out.frame)
        delete out.frame;

    int width = round(orig.getWidth() * scale_x), height = round(orig.getHeight() * scale_y);
    int newPadding = (4 - (width % 4)) % 4;
    std::cout << "new width after scaling: " << width << " height: " << height << " new padding " << newPadding << std::endl;

    out.size = ((width + newPadding) * 3) * height + 54;
    out.frame = new unsigned char[out.size];

    for (int i = 0; i < 54; i++) {
        out.frame[i] = orig.frame[i];
    }
    out.setHeight(height * -1);
    out.setWidth(width + newPadding);

    int offset = 54;
    int index = 0;
    std::cout << "yoyo\n";
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            index = ((int)(i / scale_y)) * (orig.getWidth() * 3) + (((int)(j / scale_x)) * 3) + 54;
            out.frame[offset++] = orig.frame[index + 0];
            out.frame[offset++] = orig.frame[index + 1];
            out.frame[offset++] = orig.frame[index + 2];
        }

        //std::cout << "offset " << offset << "    " << out.size << "      index " << index << "     " << orig.size << "\n";
        for (int j = 0; j < newPadding * 3; j++) {
            out.frame[offset++] = 0;
        }
    }

    std::cout << "offset\n";
    return true;

}

void highlightObjects(const struct CropedBoxList& cropedBoxes, struct Image& image_with_boxes) {
    struct CropedBox* tempBox = cropedBoxes.list;

    for (int i = 0; i < cropedBoxes.size; i++) {

        for (int j = tempBox->up; j < tempBox->up + tempBox->getHeight(); j++) {
            //left boundary
            image_with_boxes.frame[j * 960 + (tempBox->left * 3) + 54] = 0;
            image_with_boxes.frame[j * 960 + (tempBox->left * 3) + 54 + 1] = 255;
            image_with_boxes.frame[j * 960 + (tempBox->left * 3) + 54 + 2] = 0;

            //right boundary
            image_with_boxes.frame[j * 960 + (tempBox->right * 3) + 54] = 0;
            image_with_boxes.frame[j * 960 + (tempBox->right * 3) + 54 + 1] = 255;
            image_with_boxes.frame[j * 960 + (tempBox->right * 3) + 54 + 2] = 0;
        }


        for (int j = tempBox->left; j < tempBox->left + tempBox->getWidth(); j++) {
            //up boundary
            image_with_boxes.frame[tempBox->up * 960 + (j * 3) + 54] = 0;
            image_with_boxes.frame[tempBox->up * 960 + (j * 3) + 54 + 1] = 255;
            image_with_boxes.frame[tempBox->up * 960 + (j * 3) + 54 + 2] = 0;

            //down boundary
            image_with_boxes.frame[tempBox->down * 960 + (j * 3) + 54] = 0;
            image_with_boxes.frame[tempBox->down * 960 + (j * 3) + 54 + 1] = 255;
            image_with_boxes.frame[tempBox->down * 960 + (j * 3) + 54 + 2] = 0;
        }

        tempBox = tempBox->next;
    }

}

void croppedImage(const struct Image& foreground, struct CropedBox*& box, struct Image* out) {
    std::cout << "5\n";
    if (out->frame)
        delete[] out->frame;

    std::cout << "6\n";
    int widthPadding = (4 - box->getWidth() % 4) % 4;

    out->size = ((box->getWidth() + widthPadding) * 3) * box->getHeight() + 54;
    std::cout << "7  " << out->size << "\n";
    out->frame = new unsigned char[out->size];

    std::cout << "8\n";
    std::cout << out->size << "\n";
    for (int i = 0; i < 54; i++) {
        out->frame[i] = foreground.frame[i];
    }
    std::cout << "9\n";

    out->setHeight(box->getHeight() * -1);
    out->setWidth(box->getWidth() + widthPadding);

    std::cout << "After setting\n";
    std::cout << "width: " << *((int*)&out->frame[18]) << std::endl;
    std::cout << "height: " << *((int*)&out->frame[22]) << std::endl;
    std::cout << "Size:  " << out->size << "\n";

    int crpIndex = 54;
    int rowSize, offset;
    for (int i = box->up; i < box->up + box->getHeight(); i++) {
        offset = (i * 960) + 54;
        rowSize = (box->left + box->getWidth()) * 3;
        for (int j = box->left * 3; j < rowSize; j++) {
            out->frame[crpIndex++] = foreground.frame[offset + j];
        }

        for (int j = 0; j < widthPadding * 3; j++) {
            out->frame[crpIndex++] = 0;
        }
    }
    std::cout << crpIndex << "\n";


    std::cout << "H5\n";
}