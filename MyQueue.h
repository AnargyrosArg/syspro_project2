#include <vector>

template <typename T> class Queue{
private:
    std::vector<T> data;
public:
    Queue();
    ~Queue();
    void push(T);
    T pop();
    bool empty();
    int size();
};


template <typename T> Queue<T>::Queue(){
   data = std::vector<T>();
}

template <typename T> Queue<T>::~Queue(){
}

template <typename T> void Queue<T>::push(T object){
    data.emplace_back(object);
}

template <typename T> int Queue<T>::size(){
    return data.size();
}
template <typename T> T Queue<T>::pop(){
    if(data.empty()){
        std::cout << "Pop in empty queue"<<std::endl;
        exit(6);
    }else{
        T temp = data.front();
        data.erase(data.begin());
        return temp;
    }
        
}

template <typename T> bool Queue<T>::empty(){
    return data.empty();
}