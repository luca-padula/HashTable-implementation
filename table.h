#include <string>
#include <utility>
using namespace std;

template <class TYPE>
class Table {
public:
	Table() {}
	virtual void update(const string& key, const TYPE& value) = 0;
	virtual bool remove(const string& key) = 0;
	virtual bool find(const string& key, TYPE& value) = 0;
	virtual int numRecords() const = 0;
	virtual bool isEmpty() const = 0;
	virtual ~Table() {}
};

template <class TYPE>
class LPTable :public Table<TYPE> {
	struct Record
	{
		TYPE data_;
		string key_;
		// This function accepts a reference to a string representing a record key and a reference to a templated
		// object representing the record data. It creates a new record with these values.
		Record(const string& key, const TYPE& data)
		{
			key_ = key;
			data_ = data;
		}
	};
	Record** records_;
	char* recordStatus_; // status of record for tombstoning: E for empty, O for occupied, D for deleted
	int max_;
	int size_;
	int capacity_;
	std::hash<std::string> hashFunction; // standard C++ hash function used for generating a hash index

	// This function accepts a reference to a string representing a record key and a reference to an unsigned whole number
	// for passing back a found index and searches for a record with the given key.
	// If the record is found, it sets the resultIdx parameter to the index at which the record is found and returns true,
	// otherwise, it returns false.
	bool search(const string& key, size_t& resultIdx) {
		bool found = false;
		size_t hashIdx = hashFunction(key);
		size_t currIdx;
		while (!found && recordStatus_[hashIdx % capacity_] != 'E') {
			currIdx = hashIdx % capacity_;
			if (records_[currIdx] && records_[currIdx]->key_ == key) {
				resultIdx = currIdx;
				found = true;
			}
			++hashIdx;
		}
		return found;
	}

	// This function accepts an unsigned whole number representing the record's generated hash index, a reference to a string
	// for the record's key, and a reference to a templated object for the record's value.
	// It inserts the new record in the first available spot starting at the record's suggested hash index.
	void insert(size_t hashIdx, const string& key, const TYPE& value) {
		bool inserted = false;
		size_t currIdx;
		while (!inserted) {
			currIdx = hashIdx % capacity_;
			if (recordStatus_[currIdx] != 'O') {
				records_[currIdx] = new Record(key, value);
				recordStatus_[currIdx] = 'O';
				inserted = true;
			}
			++hashIdx;
		}
	}

	// This function doubles the record capacity of the table.
	void grow() {
		Record** tmpRecords = records_;
		char* tmpStatus = recordStatus_;		
		int oldCapacity = capacity_;
		capacity_ *= 2;
		max_ *= 2;
		records_ = new Record * [capacity_];
		recordStatus_ = new char[capacity_];
		for (int i = 0; i < capacity_; ++i) {
			records_[i] = nullptr;
			recordStatus_[i] = 'E';
		}
		for (int i = 0; i < oldCapacity; ++i) {
			if (tmpRecords[i]) {
				size_t hashIdx = hashFunction(tmpRecords[i]->key_);
				insert(hashIdx, tmpRecords[i]->key_, tmpRecords[i]->data_);
			}
		}
		for (int i = 0; i < oldCapacity; ++i) {
			if (tmpRecords[i]) {
				delete tmpRecords[i];
			}
		}
		delete[] tmpRecords;
		delete[] tmpStatus;
	}

public:
	LPTable(int capacity, double maxLoadFactor);
	LPTable(const LPTable& other);
	LPTable(LPTable&& other);
	virtual void update(const string& key, const TYPE& value);
	virtual bool remove(const string& key);
	virtual bool find(const string& key, TYPE& value);
	virtual const LPTable& operator=(const LPTable& other);
	virtual const LPTable& operator=(LPTable&& other);
	virtual ~LPTable();
	// This function checks if the table is empty.
	virtual bool isEmpty() const { 
		return (size_ == 0);
	}
	// This function returns the number of populated records in the table.
	virtual int numRecords() const {
		return size_;
	}

};

// This function accepts an integer representing the capacity of the table and a double representing the percentage
// of the table capacity that can be populated before it must be resized and creates a new table.
template <class TYPE>
LPTable<TYPE>::LPTable(int capacity, double maxLoadFactor) : Table<TYPE>() {
	capacity_ = capacity;
	size_ = 0;
	max_ = capacity_ * maxLoadFactor;
	records_ = new Record*[capacity_];
	recordStatus_ = new char[capacity_];
	for (int index = 0; index < capacity_; index++) {
		records_[index] = nullptr;
		recordStatus_[index] = 'E';
	}
}

// This function accepts a table and creates a copy of it.
template <class TYPE>
LPTable<TYPE>::LPTable(const LPTable<TYPE>& other) {
	capacity_ = other.capacity_;
	max_ = other.max_;
	size_ = other.size_;
	records_ = new Record * [other.capacity_];
	recordStatus_ = new char[other.capacity_];
	for (int i = 0; i < capacity_; i++) {
		if (other.records_[i] != nullptr) {
			records_[i] = new Record(other.records_[i]->key_, other.records_[i]->data_);
		}
		else {
			records_[i] = nullptr;
		}
		recordStatus_[i] = other.recordStatus_[i];
	}
}

// This function accepts a table and moves its members into a newly created table.
// The source table is not usable after this operation.
template <class TYPE>
LPTable<TYPE>::LPTable(LPTable<TYPE>&& other) {
	max_ = other.max_;
	size_ = other.size_;
	capacity_ = other.capacity_;
	records_ = other.records_;
	recordStatus_ = other.recordStatus_;

	other.max_ = 0;
	other.size_ = 0;
	other.capacity_ = 0;
	other.records_ = nullptr;
	other.recordStatus_ = nullptr;
}

// This function accepts a reference to a string representing a record key and a reference to a templated
// object representing a record value.
// If the record exists in the table, it updates its data to the value parameter.
// Otherwise, it adds a new record with the parameter values and, if the number of records in the table exceeds
// the maximum load factor, it resizes the table.
template <class TYPE>
void LPTable<TYPE>::update(const string& key, const TYPE& value) {
	size_t index_;
	if (search(key, index_)) {
		records_[index_]->data_ = value;
	}
	else {
		size_t hashIdx = hashFunction(key);
		insert(hashIdx, key, value);
		++size_;
		if (size_ > max_) {
			grow();
		}
	}
}

// This function accepts a reference to a string representing a record key.
// If the record exists in the table, it removes it and returns true.
// Otherwise, it returns false.
template <class TYPE>
bool LPTable<TYPE>::remove(const string& key) {
	size_t rmIdx;
	if (search(key, rmIdx)) {
		delete records_[rmIdx];
		records_[rmIdx] = nullptr;
		recordStatus_[rmIdx] = 'D';
		--size_;
		return true;
	}
	return false;
}

// This function accepts a reference to a string representing a record key and a reference to a templated
// object for passing back a found record data.
// If the record exists in the table, it sets the value parameter to the found record's data and returns true.
// Otherwise, it returns false.
template <class TYPE>
bool LPTable<TYPE>::find(const string& key, TYPE& value) {
	size_t index_;
	if (search(key, index_)) {
		value = records_[index_]->data_;
		return true;
	}
	return false;
}

// This function accepts a table and modifies the current table, making it a copy of other.
// It returns the newly modified table.
template <class TYPE>
const LPTable<TYPE>& LPTable<TYPE>::operator=(const LPTable<TYPE>& other) {
	if (this != &other) {

		for (int i = 0; i < capacity_; ++i) {
			if (records_[i]) {
				delete records_[i];
			}
		}
		if (records_) {
			delete[] records_;
		}
		if (recordStatus_) {
			delete[] recordStatus_;
		}
		capacity_ = other.capacity_;
		max_ = other.max_;
		size_ = other.size_;
		records_ = new Record*[capacity_];
		recordStatus_ = new char[capacity_];
		for (int i = 0; i < capacity_; i++) {
			if (other.records_[i] != nullptr) {
				records_[i] = new Record(other.records_[i]->key_, other.records_[i]->data_);
			}
			else {
				records_[i] = nullptr;
			}
			recordStatus_[i] = other.recordStatus_[i];
		}

	}
	return *this;
}

// This function accepts a table and moves its members into an existing table.
// It returns the newly modified table. The source table is not usable after this operation.
template <class TYPE>
const LPTable<TYPE>& LPTable<TYPE>::operator=(LPTable<TYPE>&& other) {
	std::swap(max_, other.max_);
	std::swap(size_, other.size_);
	std::swap(capacity_, other.capacity_);
	std::swap(records_, other.records_);
	std::swap(recordStatus_, other.recordStatus_);
	return *this;
}

// This function destroys the current table.
template <class TYPE>
LPTable<TYPE>::~LPTable() {
	for (int i = 0; i < capacity_; ++i) {
		if (records_[i]) {
			delete records_[i];
		}
	}
	if (records_) {
		delete[] records_;
	}
	if (recordStatus_) {
		delete[] recordStatus_;
	}
}